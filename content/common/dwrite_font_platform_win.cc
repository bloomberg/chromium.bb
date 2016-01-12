// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/dwrite_font_platform_win.h"

#include <windows.h>
#include <stddef.h>
#include <stdint.h>

#include <dwrite.h>
#include <wrl/implements.h>
#include <wrl/wrappers/corewrappers.h>

#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/win/registry.h"
#include "base/win/scoped_comptr.h"
#include "content/public/common/content_switches.h"

namespace {

// Font Cache implementation short story:
// Due to our sandboxing restrictions, we cannot connect to Windows font cache
// service from Renderer and need to use DirectWrite isolated font loading
// mechanism.
// DirectWrite needs to be initialized before any of the API could be used.
// During initialization DirectWrite loads all font files and populates
// internal cache, we refer this phase as enumeration and we are trying
// to optimize this phase in our cache approach. Using cache during
// initialization will help improve on startup latency in each renderer
// instance.
// During enumeration DirectWrite reads various fragments from .ttf/.ttc
// font files. Our assumption is that these fragments are being read to
// cache information such as font families, supported sizes etc.
// For reading fragments DirectWrite calls ReadFragment of our FontFileStream
// implementation with parameters start_offset and length. We cache these
// parameters along with associated data chunk.
// Here is small example of how segments are read
// start_offset: 0, length: 16
// start_offset: 0, length: 12
// start_offset: 0, length: 117
// For better cache management we collapse segments if they overlap or are
// adjacent.

namespace mswr = Microsoft::WRL;

const char kFontKeyName[] = "font_key_name";

// We use this value to determine whether to cache file fragments
// or not. In our trials we observed that for some font files
// direct write ends up reading almost entire file during enumeration
// phase. If we don't use this percentile formula we will end up
// increasing significant cache size by caching entire file contents
// for some of the font files.
const double kMaxPercentileOfFontFileSizeToCache = 0.6;

// With current implementation we map entire shared section into memory during
// renderer startup. This causes increase in working set of Chrome. As first
// step we want to see if caching is really improving any performance for our
// users, so we are putting arbitrary limit on cache file size. There are
// multiple ways we can tune our working size, like mapping only required part
// of section at any given time.
const double kArbitraryCacheFileSizeLimit = (30 * 1024 * 1024);

// We have chosen current font file length arbitrarily. In our logic
// if we don't find file we are looking for in cache we end up loading
// that file directly from system fonts folder.
const unsigned int kMaxFontFileNameLength = 34;

const DWORD kCacheFileVersion = 103;
const DWORD kFileSignature = 0x4D4F5243; // CROM
const DWORD kMagicCompletionSignature = 0x454E4F44; // DONE

const DWORD kUndefinedDWORDS = 36;

// Make sure that all structure sizes align with 8 byte boundary otherwise
// dr. memory test may complain.
#pragma pack(push, 8)
// Cache file header, includes signature, completion bits and version.
struct CacheFileHeader {
  CacheFileHeader() {
    file_signature = kFileSignature;
    magic_completion_signature = 0;
    version = kCacheFileVersion;
    ::ZeroMemory(undefined, sizeof(undefined));
  }

  DWORD file_signature;
  DWORD magic_completion_signature;
  DWORD version;
  BYTE undefined[kUndefinedDWORDS];
};

// Entry for a particular font file within this cache.
struct CacheFileEntry {
  CacheFileEntry() {
    file_size = 0;
    entry_count = 0;
    ::ZeroMemory(file_name, sizeof(file_name));
  }

  UINT64 file_size;
  DWORD entry_count;
  wchar_t file_name[kMaxFontFileNameLength];
};

// Offsets or data chunks that are cached for particular font file.
struct CacheFileOffsetEntry {
  CacheFileOffsetEntry() {
    start_offset = 0;
    length = 0;
  }

  UINT64 start_offset;
  UINT64 length;
  /* BYTE blob_[];  // Place holder for the blob that follows. */
};
#pragma pack(pop)

bool ValidateFontCacheHeader(CacheFileHeader* header) {
  return (header->file_signature == kFileSignature &&
          header->magic_completion_signature == kMagicCompletionSignature &&
          header->version == kCacheFileVersion);
}

class FontCacheWriter;

// This class implements main interface required for loading custom font
// collection as specified by DirectWrite. We also use this class for storing
// some state information as this is one of the centralized entity.
class FontCollectionLoader
    : public mswr::RuntimeClass<mswr::RuntimeClassFlags<mswr::ClassicCom>,
                                IDWriteFontCollectionLoader> {
 public:
  FontCollectionLoader()
      : in_collection_building_mode_(false),
        create_static_cache_(false) {}

  ~FontCollectionLoader() override;

  HRESULT RuntimeClassInitialize() {
    return S_OK;
  }

  // IDWriteFontCollectionLoader methods.
  HRESULT STDMETHODCALLTYPE
  CreateEnumeratorFromKey(IDWriteFactory* factory,
                          void const* key,
                          UINT32 key_size,
                          IDWriteFontFileEnumerator** file_enumerator) override;

  // Does all the initialization for required loading fonts from registry.
  static HRESULT Initialize(IDWriteFactory* factory);

  // Returns font cache map size.
  UINT32 GetFontMapSize();

  // Returns font name string when given font index.
  base::string16 GetFontNameFromKey(UINT32 idx);

  // Loads internal structure with fonts from registry.
  bool LoadFontListFromRegistry();

  // Loads restricted web safe fonts as fallback method to registry fonts.
  bool LoadRestrictedFontList();

  // Puts class in collection building mode. In collection building mode
  // we use static cache if it is available as a look aside buffer.
  void EnableCollectionBuildingMode(bool enable);

  // Returns current state of collection building.
  bool InCollectionBuildingMode();

  // Loads static cache file.
  bool LoadCacheFile();

  // Unloads cache file and related data.
  void UnloadCacheFile();

  // Puts class in static cache creating mode. In this mode we record all
  // direct write requests and store chunks of font data.
  void EnterStaticCacheMode(const WCHAR* file_name);

  // Gets out of static cache building mode.
  void LeaveStaticCacheMode();

  // Returns if class is currently in static cache building mode.
  bool IsBuildStaticCacheMode();

  // Validates cache file for consistency.
  bool ValidateCacheFile(base::File* file);

 private:
  // Structure to represent each chunk within font file that we load in memory.
  struct CacheTableOffsetEntry {
    UINT64 start_offset;
    UINT64 length;
    BYTE* inside_file_ptr;
  };

  typedef std::vector<CacheTableOffsetEntry> OffsetVector;

  // Structure representing each font entry with cache.
  struct CacheTableEntry {
    UINT64 file_size;
    OffsetVector offset_entries;
  };

 public:
  // Returns whether file we have particular font entry within cache or not.
  bool IsFileCached(UINT32 font_key);
  // Returns cache fragment corresponding to specific font key.
  void* GetCachedFragment(UINT32 font_key, UINT64 start_offset, UINT64 length);
  // Returns actual font file size at the time of caching.
  UINT64 GetCachedFileSize(UINT32 font_key);

  // Returns instance of font cache writer. This class manages actual font
  // file format.
  FontCacheWriter* GetFontCacheWriter();

 private:
  // Functions validates and loads cache into internal map.
  bool ValidateAndLoadCacheMap();

  mswr::ComPtr<IDWriteFontFileLoader> file_loader_;

  std::vector<base::string16> reg_fonts_;
  bool in_collection_building_mode_;
  bool create_static_cache_;
  scoped_ptr<base::SharedMemory> cache_;
  scoped_ptr<FontCacheWriter> cache_writer_;

  typedef std::map<base::string16, CacheTableEntry*> CacheMap;
  CacheMap cache_map_;

  DISALLOW_COPY_AND_ASSIGN(FontCollectionLoader);
};

mswr::ComPtr<FontCollectionLoader> g_font_loader;
base::win::ScopedHandle g_shared_font_cache;

// Class responsible for handling font cache file format details as well as
// tracking various cache region requests by direct write.
class FontCacheWriter {
 public:
  FontCacheWriter() : count_font_entries_ignored_(0), cookie_counter_(0) {}

  ~FontCacheWriter() {
    if (static_cache_.get()) {
      static_cache_->Close();
    }
  }

 public:
  // Holds data related to individual region as requested by direct write.
  struct CacheRegion {
    UINT64 start_offset;
    UINT64 length;
    const BYTE* ptr;
    /* BYTE blob_[];  // Place holder for the blob that follows. */
  };

  // Function to create static font cache file.
  bool Create(const wchar_t* file_name) {
    static_cache_.reset(new base::File(base::FilePath(file_name),
        base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE |
        base::File::FLAG_EXCLUSIVE_WRITE));
    if (!static_cache_->IsValid()) {
      static_cache_.reset();
      return false;
    }
    CacheFileHeader header;

    // At offset 0 write cache version
    static_cache_->Write(0,
                         reinterpret_cast<const char*>(&header),
                         sizeof(header));

    static_cache_->Flush();
    return true;
  }

  // Closes static font cache file. Also writes completion signature to mark
  // it as completely written.
  void Close() {
    if (static_cache_.get()) {
      CacheFileHeader header;
      header.magic_completion_signature = kMagicCompletionSignature;
      // At offset 0 write cache version
      int bytes_written = static_cache_->Write(0,
          reinterpret_cast<const char*>(&header),
          sizeof(header));
      DCHECK_NE(bytes_written, -1);

      UMA_HISTOGRAM_MEMORY_KB("DirectWrite.Fonts.BuildCache.File.Size",
                              static_cache_->GetLength() / 1024);

      UMA_HISTOGRAM_COUNTS("DirectWrite.Fonts.BuildCache.Ignored",
                           count_font_entries_ignored_);

      static_cache_->Close();
      static_cache_.reset(NULL);
    }
  }

 private:
  typedef std::vector<CacheRegion> RegionVector;

  // Structure to track various regions requested by direct write for particular
  // font file.
  struct FontEntryInternal {
    FontEntryInternal(const wchar_t* name, UINT64 size)
        : file_name(name),
          file_size(size) {
    }

    base::string16 file_name;
    UINT64 file_size;
    RegionVector regions;
  };

 public:
  // Starts up new font entry to be tracked, returns cookie to identify this
  // particular entry.
  UINT NewFontEntry(const wchar_t* file_name, UINT64 file_size) {
    base::AutoLock lock(lock_);
    UINT old_counter = cookie_counter_;
    FontEntryInternal* font_entry = new FontEntryInternal(file_name, file_size);
    cookie_map_[cookie_counter_].reset(font_entry);
    cookie_counter_++;
    return old_counter;
  }

  // AddRegion function lets caller add various regions to be cached for
  // particular font file. Once enumerating that particular font file is done
  // (based on uniquely identifying cookie) changes could be committed using
  // CommitFontEntry
  bool AddRegion(UINT64 cookie, UINT64 start, UINT64 length, const BYTE* ptr) {
    base::AutoLock lock(lock_);
    if (cookie_map_.find(cookie) == cookie_map_.end())
      return false;
    RegionVector& regions = cookie_map_[cookie].get()->regions;
    CacheRegion region;
    region.start_offset = start;
    region.length = length;
    region.ptr = ptr;
    regions.push_back(region);
    return true;
  }

  // Function which commits after merging all collected regions into cache file.
  bool CommitFontEntry(UINT cookie) {
    base::AutoLock lock(lock_);
    if (cookie_map_.find(cookie) == cookie_map_.end())
      return false;

    // We will skip writing entries beyond allowed limit. Following condition
    // doesn't enforce hard file size. We need to write complete font entry.
    int64_t length = static_cache_->GetLength();
    if (length == -1 || length >= kArbitraryCacheFileSizeLimit) {
      count_font_entries_ignored_++;
      return false;
    }

    FontEntryInternal* font_entry = cookie_map_[cookie].get();
    RegionVector& regions = font_entry->regions;
    std::sort(regions.begin(), regions.end(), SortCacheRegions);

    // At this point, we have collected all regions to be cached. These regions
    // are tuples of start, length, data for particular data segment.
    // These tuples can overlap.
    // e.g. (0, 12, data), (0, 117, data), (21, 314, data), (335, 15, data)
    // In this case as you can see first three segments overlap and
    // 4th is adjacent. If we cache them individually then we will end up
    // caching duplicate data, so we merge these segments together to find
    // superset for the cache. In above example our algorithm should
    // produce (cache) single segment starting at offset 0 with length 350.
    RegionVector merged_regions;
    RegionVector::iterator iter;
    int idx = 0;
    for (iter = regions.begin(); iter != regions.end(); iter++) {
      if (iter == regions.begin()) {
        merged_regions.push_back(*iter);
        continue;
      }
      CacheRegion& base_region = merged_regions[idx];
      if (IsOverlap(&base_region, &(*iter))) {
        UINT64 end1 = base_region.start_offset + base_region.length;
        UINT64 end2 = iter->start_offset + iter->length;
        if (base_region.start_offset > iter->start_offset) {
          base_region.start_offset = iter->start_offset;
          base_region.ptr = iter->ptr;
        }
        base_region.length = std::max(end1, end2) - base_region.start_offset;
      } else {
        merged_regions.push_back(*iter);
        idx++;
      }
    }

    UINT64 total_merged_cache_in_bytes = 0;
    for (iter = merged_regions.begin(); iter != merged_regions.end(); iter++) {
      total_merged_cache_in_bytes += iter->length;
    }

    // We want to adjust following parameter based on experiments. But general
    // logic here is that if we are going to end up caching most of the contents
    // for a file (e.g. simsunb.ttf > 90%) then we should avoid caching that
    // file.
    double percentile = static_cast<double>(total_merged_cache_in_bytes) /
        font_entry->file_size;
    if (percentile > kMaxPercentileOfFontFileSizeToCache) {
      count_font_entries_ignored_++;
      return false;
    }

    CacheFileEntry entry;
    wcsncpy_s(entry.file_name, kMaxFontFileNameLength,
              font_entry->file_name.c_str(), _TRUNCATE);
    entry.file_size = font_entry->file_size;
    entry.entry_count = merged_regions.size();
    static_cache_->WriteAtCurrentPos(
        reinterpret_cast<const char*>(&entry),
        sizeof(entry));
    for (iter = merged_regions.begin(); iter != merged_regions.end(); iter++) {
      CacheFileOffsetEntry offset_entry;
      offset_entry.start_offset = iter->start_offset;
      offset_entry.length = iter->length;
      static_cache_->WriteAtCurrentPos(
          reinterpret_cast<const char*>(&offset_entry),
          sizeof(offset_entry));
      static_cache_->WriteAtCurrentPos(
          reinterpret_cast<const char*>(iter->ptr),
          iter->length);
    }
    return true;
  }

 private:
  // This is the count of font entries that we reject based on size to be
  // cached.
  unsigned int count_font_entries_ignored_;
  scoped_ptr<base::File> static_cache_;
  std::map<UINT, scoped_ptr<FontEntryInternal>> cookie_map_;
  UINT cookie_counter_;

  // Lock is required to protect internal data structures and access to file,
  // According to MSDN documentation on ReadFileFragment and based on our
  // experiments so far, there is possibility of ReadFileFragment getting called
  // from multiple threads.
  base::Lock lock_;

  // Function checks if two regions overlap or are adjacent.
  bool IsOverlap(CacheRegion* region1, CacheRegion* region2) {
    return
        !((region1->start_offset + region1->length) < region2->start_offset ||
          region1->start_offset > (region2->start_offset + region2->length));
  }

  // Function to sort cached regions.
  static bool SortCacheRegions(const CacheRegion& region1,
                               const CacheRegion& region2) {
    return
        region1.start_offset == region2.start_offset ?
            region1.length < region2.length :
            region1.start_offset < region2.start_offset;
  }

  DISALLOW_COPY_AND_ASSIGN(FontCacheWriter);
};

// Class implements IDWriteFontFileStream interface as required by direct write.
class FontFileStream
    : public mswr::RuntimeClass<mswr::RuntimeClassFlags<mswr::ClassicCom>,
                                IDWriteFontFileStream> {
 public:
  // IDWriteFontFileStream methods.
  HRESULT STDMETHODCALLTYPE ReadFileFragment(
      void const** fragment_start,
      UINT64 file_offset,
      UINT64 fragment_size,
      void** context) override {
    if (cached_data_) {
      *fragment_start = g_font_loader->GetCachedFragment(font_key_,
                                                         file_offset,
                                                         fragment_size);
      if (*fragment_start == NULL) {
        DCHECK(false);
      }
      *context = NULL;
      return *fragment_start != NULL ? S_OK : E_FAIL;
    }
    if (!memory_.get() || !memory_->IsValid() ||
        file_offset >= memory_->length() ||
        (file_offset + fragment_size) > memory_->length())
      return E_FAIL;

    *fragment_start = static_cast<BYTE const*>(memory_->data()) +
                      static_cast<size_t>(file_offset);
    *context = NULL;
    if (g_font_loader->IsBuildStaticCacheMode()) {
      FontCacheWriter* cache_writer = g_font_loader->GetFontCacheWriter();
      cache_writer->AddRegion(writer_cookie_,
                              file_offset,
                              fragment_size,
                              static_cast<const BYTE*>(*fragment_start));
    }
    return S_OK;
  }

  void STDMETHODCALLTYPE ReleaseFileFragment(void* context) override {}

  HRESULT STDMETHODCALLTYPE GetFileSize(UINT64* file_size) override {
    if (cached_data_) {
      *file_size = g_font_loader->GetCachedFileSize(font_key_);
      return S_OK;
    }

    if (!memory_.get() || !memory_->IsValid())
      return E_FAIL;

    *file_size = memory_->length();
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE GetLastWriteTime(UINT64* last_write_time) override {
    if (cached_data_) {
      *last_write_time = 0;
      return S_OK;
    }

    if (!memory_.get() || !memory_->IsValid())
      return E_FAIL;

    // According to MSDN article http://goo.gl/rrSYzi the "last modified time"
    // is used by DirectWrite font selection algorithms to determine whether
    // one font resource is more up to date than another one.
    // So by returning 0 we are assuming that it will treat all fonts to be
    // equally up to date.
    // TODO(shrikant): We should further investigate this.
    *last_write_time = 0;
    return S_OK;
  }

  FontFileStream() : font_key_(0), cached_data_(false) {}

  HRESULT RuntimeClassInitialize(UINT32 font_key) {
    if (g_font_loader->InCollectionBuildingMode() &&
        g_font_loader->IsFileCached(font_key)) {
      cached_data_ = true;
      font_key_ = font_key;
      return S_OK;
    }

    base::FilePath path;
    PathService::Get(base::DIR_WINDOWS_FONTS, &path);
    base::string16 font_key_name(g_font_loader->GetFontNameFromKey(font_key));
    path = path.Append(font_key_name.c_str());
    memory_.reset(new base::MemoryMappedFile());

    // Put some debug information on stack.
    WCHAR font_name[MAX_PATH];
    path.value().copy(font_name, arraysize(font_name));
    base::debug::Alias(font_name);

    if (!memory_->Initialize(path)) {
      memory_.reset();
      return E_FAIL;
    }

    font_key_ = font_key;

    base::debug::SetCrashKeyValue(kFontKeyName,
                                  base::WideToUTF8(font_key_name));

    if (g_font_loader->IsBuildStaticCacheMode()) {
      FontCacheWriter* cache_writer = g_font_loader->GetFontCacheWriter();
      writer_cookie_ = cache_writer->NewFontEntry(font_key_name.c_str(),
                                                  memory_->length());
    }
    return S_OK;
  }

  ~FontFileStream() override {
    if (g_font_loader->IsBuildStaticCacheMode()) {
      FontCacheWriter* cache_writer = g_font_loader->GetFontCacheWriter();
      cache_writer->CommitFontEntry(writer_cookie_);
    }
  }

 private:
  UINT32 font_key_;
  scoped_ptr<base::MemoryMappedFile> memory_;
  bool cached_data_;
  UINT writer_cookie_;

  DISALLOW_COPY_AND_ASSIGN(FontFileStream);
};

// Implements IDWriteFontFileLoader as required by FontFileLoader.
class FontFileLoader
    : public mswr::RuntimeClass<mswr::RuntimeClassFlags<mswr::ClassicCom>,
                                IDWriteFontFileLoader> {
 public:
  // IDWriteFontFileLoader methods.
  HRESULT STDMETHODCALLTYPE
  CreateStreamFromKey(void const* ref_key,
                      UINT32 ref_key_size,
                      IDWriteFontFileStream** stream) override {
    if (ref_key_size != sizeof(UINT32))
      return E_FAIL;

    UINT32 font_key = *static_cast<const UINT32*>(ref_key);
    mswr::ComPtr<FontFileStream> font_stream;
    HRESULT hr = mswr::MakeAndInitialize<FontFileStream>(&font_stream,
                                                         font_key);
    if (SUCCEEDED(hr)) {
      *stream = font_stream.Detach();
      return S_OK;
    }
    return E_FAIL;
  }

  FontFileLoader() {}
  ~FontFileLoader() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FontFileLoader);
};

// Implements IDWriteFontFileEnumerator as required by direct write.
class FontFileEnumerator
    : public mswr::RuntimeClass<mswr::RuntimeClassFlags<mswr::ClassicCom>,
                                IDWriteFontFileEnumerator> {
 public:
  // IDWriteFontFileEnumerator methods.
  HRESULT STDMETHODCALLTYPE MoveNext(BOOL* has_current_file) override {
    *has_current_file = FALSE;

    if (current_file_)
      current_file_.ReleaseAndGetAddressOf();

    if (font_idx_ < g_font_loader->GetFontMapSize()) {
      HRESULT hr =
          factory_->CreateCustomFontFileReference(&font_idx_,
                                                  sizeof(UINT32),
                                                  file_loader_.Get(),
                                                  current_file_.GetAddressOf());
      DCHECK(SUCCEEDED(hr));
      *has_current_file = TRUE;
      font_idx_++;
    }
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  GetCurrentFontFile(IDWriteFontFile** font_file) override {
    if (!current_file_) {
      *font_file = NULL;
      return E_FAIL;
    }

    *font_file = current_file_.Detach();
    return S_OK;
  }

  FontFileEnumerator(const void* keys,
                     UINT32 buffer_size,
                     IDWriteFactory* factory,
                     IDWriteFontFileLoader* file_loader)
      : factory_(factory), file_loader_(file_loader), font_idx_(0) {}

  ~FontFileEnumerator() override {}

  mswr::ComPtr<IDWriteFactory> factory_;
  mswr::ComPtr<IDWriteFontFile> current_file_;
  mswr::ComPtr<IDWriteFontFileLoader> file_loader_;
  UINT32 font_idx_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FontFileEnumerator);
};

// IDWriteFontCollectionLoader methods.
HRESULT STDMETHODCALLTYPE FontCollectionLoader::CreateEnumeratorFromKey(
    IDWriteFactory* factory,
    void const* key,
    UINT32 key_size,
    IDWriteFontFileEnumerator** file_enumerator) {
  *file_enumerator = mswr::Make<FontFileEnumerator>(
                         key, key_size, factory, file_loader_.Get()).Detach();
  return S_OK;
}

// static
HRESULT FontCollectionLoader::Initialize(IDWriteFactory* factory) {
  DCHECK(g_font_loader == NULL);

  HRESULT result;
  result = mswr::MakeAndInitialize<FontCollectionLoader>(&g_font_loader);
  if (FAILED(result) || !g_font_loader) {
    DCHECK(false);
    return E_FAIL;
  }

  CHECK(g_font_loader->LoadFontListFromRegistry());

  g_font_loader->file_loader_ = mswr::Make<FontFileLoader>().Detach();

  factory->RegisterFontFileLoader(g_font_loader->file_loader_.Get());
  factory->RegisterFontCollectionLoader(g_font_loader.Get());

  return S_OK;
}

FontCollectionLoader::~FontCollectionLoader() {
  STLDeleteContainerPairSecondPointers(cache_map_.begin(), cache_map_.end());
}

UINT32 FontCollectionLoader::GetFontMapSize() {
  return reg_fonts_.size();
}

base::string16 FontCollectionLoader::GetFontNameFromKey(UINT32 idx) {
  DCHECK(idx < reg_fonts_.size());
  return reg_fonts_[idx];
}

const base::FilePath::CharType* kFontExtensionsToIgnore[] {
  FILE_PATH_LITERAL(".FON"),  // Bitmap or vector
  FILE_PATH_LITERAL(".PFM"),  // Adobe Type 1
  FILE_PATH_LITERAL(".PFB"),  // Adobe Type 1
};

const wchar_t* kFontsToIgnore[] = {
  // "Gill Sans Ultra Bold" turns into an Ultra Bold weight "Gill Sans" in
  // DirectWrite, but most users don't have any other weights. The regular
  // weight font is named "Gill Sans MT", but that ends up in a different
  // family with that name. On Mac, there's a "Gill Sans" with various weights,
  // so CSS authors use { 'font-family': 'Gill Sans', 'Gill Sans MT', ... } and
  // because of the DirectWrite family futzing, they end up with an Ultra Bold
  // font, when they just wanted "Gill Sans". Mozilla implemented a more
  // complicated hack where they effectively rename the Ultra Bold font to
  // "Gill Sans MT Ultra Bold", but because the Ultra Bold font is so ugly
  // anyway, we simply ignore it. See
  // http://www.microsoft.com/typography/fonts/font.aspx?FMID=978 for a picture
  // of the font, and the file name. We also ignore "Gill Sans Ultra Bold
  // Condensed".
  L"gilsanub.ttf",
  L"gillubcd.ttf",
};

bool FontCollectionLoader::LoadFontListFromRegistry() {
  const wchar_t kFontsRegistry[] =
      L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";
  CHECK(reg_fonts_.empty());
  base::win::RegKey regkey;
  if (regkey.Open(HKEY_LOCAL_MACHINE, kFontsRegistry, KEY_READ) !=
      ERROR_SUCCESS) {
    return false;
  }

  base::FilePath system_font_path;
  PathService::Get(base::DIR_WINDOWS_FONTS, &system_font_path);

  base::string16 name;
  base::string16 value;
  for (DWORD idx = 0; idx < regkey.GetValueCount(); idx++) {
    if (regkey.GetValueNameAt(idx, &name) == ERROR_SUCCESS &&
        regkey.ReadValue(name.c_str(), &value) == ERROR_SUCCESS) {
      base::FilePath path(value.c_str());
      // We need to check if path in registry is absolute, if it is then
      // we check if it is same as DIR_WINDOWS_FONTS otherwise we ignore.
      bool absolute = path.IsAbsolute();
      if (absolute &&
          !base::FilePath::CompareEqualIgnoreCase(system_font_path.value(),
                                                  path.DirName().value())) {
        continue;
      }

      // Ignore if path ends with a separator.
      if (path.EndsWithSeparator())
        continue;

      if (absolute)
        value = path.BaseName().value();

      bool should_ignore = false;
      for (const auto& ignore : kFontsToIgnore) {
        if (base::FilePath::CompareEqualIgnoreCase(value, ignore)) {
          should_ignore = true;
          break;
        }
      }
      // DirectWrite doesn't support bitmap/vector fonts and Adobe type 1
      // fonts, we will ignore those font extensions.
      // MSDN article: http://goo.gl/TfCOA
      if (!should_ignore) {
        for (const auto& ignore : kFontExtensionsToIgnore) {
          if (path.MatchesExtension(ignore)) {
            should_ignore = true;
            break;
          }
        }
      }

      if (!should_ignore)
        reg_fonts_.push_back(value.c_str());
    }
  }
  UMA_HISTOGRAM_COUNTS("DirectWrite.Fonts.Loaded", reg_fonts_.size());
  UMA_HISTOGRAM_COUNTS("DirectWrite.Fonts.Ignored",
                       regkey.GetValueCount() - reg_fonts_.size());
  return true;
}

// This list is mainly based on prefs/prefs_tab_helper.cc kFontDefaults.
const wchar_t* kRestrictedFontSet[] = {
  // These are the "Web Safe" fonts.
  L"times.ttf",     // IDS_STANDARD_FONT_FAMILY
  L"timesbd.ttf",   // IDS_STANDARD_FONT_FAMILY
  L"timesbi.ttf",   // IDS_STANDARD_FONT_FAMILY
  L"timesi.ttf",    // IDS_STANDARD_FONT_FAMILY
  L"cour.ttf",      // IDS_FIXED_FONT_FAMILY
  L"courbd.ttf",    // IDS_FIXED_FONT_FAMILY
  L"courbi.ttf",    // IDS_FIXED_FONT_FAMILY
  L"couri.ttf",     // IDS_FIXED_FONT_FAMILY
  L"consola.ttf",   // IDS_FIXED_FONT_FAMILY_ALT_WIN
  L"consolab.ttf",  // IDS_FIXED_FONT_FAMILY_ALT_WIN
  L"consolai.ttf",  // IDS_FIXED_FONT_FAMILY_ALT_WIN
  L"consolaz.ttf",  // IDS_FIXED_FONT_FAMILY_ALT_WIN
  L"arial.ttf",     // IDS_SANS_SERIF_FONT_FAMILY
  L"arialbd.ttf",   // IDS_SANS_SERIF_FONT_FAMILY
  L"arialbi.ttf",   // IDS_SANS_SERIF_FONT_FAMILY
  L"ariali.ttf",    // IDS_SANS_SERIF_FONT_FAMILY
  L"comic.ttf",     // IDS_CURSIVE_FONT_FAMILY
  L"comicbd.ttf",   // IDS_CURSIVE_FONT_FAMILY
  L"comici.ttf",    // IDS_CURSIVE_FONT_FAMILY
  L"comicz.ttf",    // IDS_CURSIVE_FONT_FAMILY
  L"impact.ttf",    // IDS_FANTASY_FONT_FAMILY
  L"georgia.ttf",
  L"georgiab.ttf",
  L"georgiai.ttf",
  L"georgiaz.ttf",
  L"trebuc.ttf",
  L"trebucbd.ttf",
  L"trebucbi.ttf",
  L"trebucit.ttf",
  L"verdana.ttf",
  L"verdanab.ttf",
  L"verdanai.ttf",
  L"verdanaz.ttf",
  L"segoeui.ttf",   // IDS_PICTOGRAPH_FONT_FAMILY
  L"segoeuib.ttf",  // IDS_PICTOGRAPH_FONT_FAMILY
  L"segoeuii.ttf",  // IDS_PICTOGRAPH_FONT_FAMILY
  L"msgothic.ttc",  // IDS_STANDARD_FONT_FAMILY_JAPANESE
  L"msmincho.ttc",  // IDS_SERIF_FONT_FAMILY_JAPANESE
  L"gulim.ttc",     // IDS_FIXED_FONT_FAMILY_KOREAN
  L"batang.ttc",    // IDS_SERIF_FONT_FAMILY_KOREAN
  L"simsun.ttc",    // IDS_STANDARD_FONT_FAMILY_SIMPLIFIED_HAN
  L"mingliu.ttc",   // IDS_SERIF_FONT_FAMILY_TRADITIONAL_HAN

  // These are from the Blink fallback list.
  L"david.ttf",     // USCRIPT_HEBREW
  L"davidbd.ttf",   // USCRIPT_HEBREW
  L"euphemia.ttf",  // USCRIPT_CANADIAN_ABORIGINAL
  L"gautami.ttf",   // USCRIPT_TELUGU
  L"gautamib.ttf",  // USCRIPT_TELUGU
  L"latha.ttf",     // USCRIPT_TAMIL
  L"lathab.ttf",    // USCRIPT_TAMIL
  L"mangal.ttf",    // USCRIPT_DEVANAGARI
  L"mangalb.ttf",   // USCRIPT_DEVANAGARI
  L"monbaiti.ttf",  // USCRIPT_MONGOLIAN
  L"mvboli.ttf",    // USCRIPT_THAANA
  L"plantc.ttf",    // USCRIPT_CHEROKEE
  L"raavi.ttf",     // USCRIPT_GURMUKHI
  L"raavib.ttf",    // USCRIPT_GURMUKHI
  L"shruti.ttf",    // USCRIPT_GUJARATI
  L"shrutib.ttf",   // USCRIPT_GUJARATI
  L"sylfaen.ttf",   // USCRIPT_GEORGIAN and USCRIPT_ARMENIAN
  L"tahoma.ttf",    // USCRIPT_ARABIC,
  L"tahomabd.ttf",  // USCRIPT_ARABIC,
  L"tunga.ttf",     // USCRIPT_KANNADA
  L"tungab.ttf",    // USCRIPT_KANNADA
  L"vrinda.ttf",    // USCRIPT_BENGALI
  L"vrindab.ttf",   // USCRIPT_BENGALI
};

bool FontCollectionLoader::LoadRestrictedFontList() {
  reg_fonts_.clear();
  reg_fonts_.assign(kRestrictedFontSet,
                    kRestrictedFontSet + _countof(kRestrictedFontSet));
  return true;
}

void FontCollectionLoader::EnableCollectionBuildingMode(bool enable) {
  in_collection_building_mode_ = enable;
}

bool FontCollectionLoader::InCollectionBuildingMode() {
  return  in_collection_building_mode_;
}

bool FontCollectionLoader::IsFileCached(UINT32 font_key) {
  if (!cache_.get() || cache_->memory() == NULL) {
    return false;
  }
  CacheMap::iterator iter = cache_map_.find(
      GetFontNameFromKey(font_key).c_str());
  return iter != cache_map_.end();
}

bool FontCollectionLoader::LoadCacheFile() {
  TRACE_EVENT0("startup", "FontCollectionLoader::LoadCacheFile");

  std::string font_cache_handle_string =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kFontCacheSharedHandle);
  if (font_cache_handle_string.empty())
    return false;

  unsigned int handle_uint;
  base::StringToUint(font_cache_handle_string, &handle_uint);
  DCHECK(handle_uint);
  if (handle_uint > static_cast<unsigned int>(std::numeric_limits<long>::max()))
    return false;
  base::SharedMemoryHandle font_cache_handle(LongToHandle(handle_uint),
                                             base::GetCurrentProcId());

  base::SharedMemory* shared_mem = new base::SharedMemory(
      font_cache_handle, true);
  // Map the cache file into memory.
  shared_mem->Map(0);

  cache_.reset(shared_mem);

  if (base::StartsWith(base::FieldTrialList::FindFullName("LightSpeed"),
                       "PrefetchDWriteFontCache",
                       base::CompareCase::SENSITIVE)) {
    // Prefetch the cache, to avoid unordered IO when it is used.
    // PrefetchVirtualMemory() is loaded dynamically because it is only
    // available from Win8.
    decltype(PrefetchVirtualMemory)* prefetch_virtual_memory =
        reinterpret_cast<decltype(PrefetchVirtualMemory)*>(::GetProcAddress(
            ::GetModuleHandle(L"kernel32.dll"), "PrefetchVirtualMemory"));
    if (prefetch_virtual_memory != NULL) {
      WIN32_MEMORY_RANGE_ENTRY memory_range;
      memory_range.VirtualAddress = shared_mem->memory();
      memory_range.NumberOfBytes = shared_mem->mapped_size();
      prefetch_virtual_memory(::GetCurrentProcess(), 1, &memory_range, 0);
    }
  }

  if (!ValidateAndLoadCacheMap()) {
    cache_.reset();
    return false;
  }

  return true;
}

void FontCollectionLoader::UnloadCacheFile() {
  cache_.reset();
  STLDeleteContainerPairSecondPointers(cache_map_.begin(), cache_map_.end());
  cache_map_.clear();
}

void FontCollectionLoader::EnterStaticCacheMode(const WCHAR* file_name) {
  cache_writer_.reset(new FontCacheWriter());
  if (cache_writer_->Create(file_name))
    create_static_cache_ = true;
}

void FontCollectionLoader::LeaveStaticCacheMode() {
  cache_writer_->Close();
  cache_writer_.reset(NULL);
  create_static_cache_ = false;
}

bool FontCollectionLoader::IsBuildStaticCacheMode() {
  return create_static_cache_;
}

bool FontCollectionLoader::ValidateAndLoadCacheMap() {
  BYTE* mem_file_start = static_cast<BYTE*>(cache_->memory());
  BYTE* mem_file_end = mem_file_start + cache_->mapped_size();

  BYTE* current_ptr = mem_file_start;
  CacheFileHeader* file_header =
      reinterpret_cast<CacheFileHeader*>(current_ptr);
  if (!ValidateFontCacheHeader(file_header))
    return false;

  current_ptr = current_ptr + sizeof(CacheFileHeader);
  if (current_ptr >= mem_file_end)
    return false;

  while ((current_ptr + sizeof(CacheFileEntry)) < mem_file_end) {
    CacheFileEntry* entry = reinterpret_cast<CacheFileEntry*>(current_ptr);
    current_ptr += sizeof(CacheFileEntry);
    WCHAR file_name[kMaxFontFileNameLength];
    wcsncpy_s(file_name,
              kMaxFontFileNameLength,
              entry->file_name,
              _TRUNCATE);
    CacheTableEntry* table_entry = NULL;
    CacheMap::iterator iter = cache_map_.find(file_name);
    if (iter == cache_map_.end()) {
      table_entry = new CacheTableEntry();
      cache_map_[file_name] = table_entry;
    } else {
      table_entry = iter->second;
    }
    table_entry->file_size = entry->file_size;
    for (DWORD idx = 0;
         (current_ptr + sizeof(CacheFileOffsetEntry)) < mem_file_end &&
         idx < entry->entry_count;
         idx++) {
      CacheFileOffsetEntry* offset_entry =
          reinterpret_cast<CacheFileOffsetEntry*>(current_ptr);
      CacheTableOffsetEntry table_offset_entry;
      table_offset_entry.start_offset = offset_entry->start_offset;
      table_offset_entry.length = offset_entry->length;
      table_offset_entry.inside_file_ptr =
          current_ptr + sizeof(CacheFileOffsetEntry);
      table_entry->offset_entries.push_back(table_offset_entry);
      current_ptr += sizeof(CacheFileOffsetEntry);
      current_ptr += offset_entry->length;
    }
  }

  return true;
}

void* FontCollectionLoader::GetCachedFragment(UINT32 font_key,
                                              UINT64 start_offset,
                                              UINT64 length) {
  UINT64 just_past_end = start_offset + length;
  CacheMap::iterator iter = cache_map_.find(
      GetFontNameFromKey(font_key).c_str());
  if (iter != cache_map_.end()) {
    CacheTableEntry* entry = iter->second;
    OffsetVector::iterator offset_iter = entry->offset_entries.begin();
    while (offset_iter != entry->offset_entries.end()) {
      UINT64 available_just_past_end =
          offset_iter->start_offset + offset_iter->length;
      if (offset_iter->start_offset <= start_offset &&
          just_past_end <= available_just_past_end) {
        return offset_iter->inside_file_ptr +
            (start_offset - offset_iter->start_offset);
      }
      offset_iter++;
    }
  }
  return NULL;
}

UINT64 FontCollectionLoader::GetCachedFileSize(UINT32 font_key) {
  CacheMap::iterator iter = cache_map_.find(
      GetFontNameFromKey(font_key).c_str());
  if (iter != cache_map_.end()) {
    return iter->second->file_size;
  }
  return 0;
}

FontCacheWriter* FontCollectionLoader::GetFontCacheWriter() {
  return cache_writer_.get();
}

}  // namespace

namespace content {

const char kFontCacheSharedSectionName[] = "ChromeDWriteFontCache";

mswr::ComPtr<IDWriteFontCollection> g_font_collection;

IDWriteFontCollection* GetCustomFontCollection(IDWriteFactory* factory) {
  if (g_font_collection.Get() != NULL)
    return g_font_collection.Get();

  TRACE_EVENT0("startup", "content::GetCustomFontCollection");

  base::TimeTicks start_tick = base::TimeTicks::Now();

  FontCollectionLoader::Initialize(factory);

  bool cache_file_loaded = g_font_loader->LoadCacheFile();

  // Arbitrary threshold to stop loading enormous number of fonts. Usual
  // side effect of loading large number of fonts results in renderer getting
  // killed as it appears to hang.
  const UINT32 kMaxFontThreshold = 1750;
  HRESULT hr = E_FAIL;
  if (cache_file_loaded ||
      g_font_loader->GetFontMapSize() < kMaxFontThreshold) {
    g_font_loader->EnableCollectionBuildingMode(true);
    hr = factory->CreateCustomFontCollection(
        g_font_loader.Get(), NULL, 0, g_font_collection.GetAddressOf());
    g_font_loader->UnloadCacheFile();
    g_font_loader->EnableCollectionBuildingMode(false);
  }
  bool loading_restricted = false;
  if (FAILED(hr) || !g_font_collection.Get()) {
    loading_restricted = true;
    // We will try here just one more time with restricted font set.
    g_font_loader->LoadRestrictedFontList();
    hr = factory->CreateCustomFontCollection(
        g_font_loader.Get(), NULL, 0, g_font_collection.GetAddressOf());
  }

  base::TimeDelta time_delta = base::TimeTicks::Now() - start_tick;
  int64_t delta = time_delta.ToInternalValue();
  base::debug::Alias(&delta);
  UINT32 size = g_font_loader->GetFontMapSize();
  base::debug::Alias(&size);
  base::debug::Alias(&loading_restricted);

  CHECK(SUCCEEDED(hr));
  CHECK(g_font_collection.Get() != NULL);

  if (cache_file_loaded)
    UMA_HISTOGRAM_TIMES("DirectWrite.Fonts.LoadTime.Cached", time_delta);
  else
    UMA_HISTOGRAM_TIMES("DirectWrite.Fonts.LoadTime", time_delta);

  base::debug::ClearCrashKey(kFontKeyName);

  return g_font_collection.Get();
}

bool BuildFontCacheInternal(const WCHAR* file_name) {
  typedef decltype(DWriteCreateFactory)* DWriteCreateFactoryProc;
  HMODULE dwrite_dll = LoadLibraryW(L"dwrite.dll");
  if (!dwrite_dll) {
    DWORD load_library_get_last_error = GetLastError();
    base::debug::Alias(&dwrite_dll);
    base::debug::Alias(&load_library_get_last_error);
    CHECK(false);
  }

  DWriteCreateFactoryProc dwrite_create_factory_proc =
      reinterpret_cast<DWriteCreateFactoryProc>(
          GetProcAddress(dwrite_dll, "DWriteCreateFactory"));

  if (!dwrite_create_factory_proc) {
    DWORD get_proc_address_get_last_error = GetLastError();
    base::debug::Alias(&dwrite_create_factory_proc);
    base::debug::Alias(&get_proc_address_get_last_error);
    CHECK(false);
  }

  mswr::ComPtr<IDWriteFactory> factory;

  CHECK(SUCCEEDED(
      dwrite_create_factory_proc(
          DWRITE_FACTORY_TYPE_ISOLATED,
          __uuidof(IDWriteFactory),
          reinterpret_cast<IUnknown**>(factory.GetAddressOf()))));

  base::TimeTicks start_tick = base::TimeTicks::Now();

  FontCollectionLoader::Initialize(factory.Get());

  g_font_loader->EnterStaticCacheMode(file_name);

  mswr::ComPtr<IDWriteFontCollection> font_collection;

  HRESULT hr = E_FAIL;
  g_font_loader->EnableCollectionBuildingMode(true);
  hr = factory->CreateCustomFontCollection(
      g_font_loader.Get(), NULL, 0, font_collection.GetAddressOf());
  g_font_loader->EnableCollectionBuildingMode(false);

  bool loading_restricted = false;
  if (FAILED(hr) || !font_collection.Get()) {
    loading_restricted = true;
    // We will try here just one more time with restricted font set.
    g_font_loader->LoadRestrictedFontList();
    hr = factory->CreateCustomFontCollection(
        g_font_loader.Get(), NULL, 0, font_collection.GetAddressOf());
  }

  g_font_loader->LeaveStaticCacheMode();

  base::TimeDelta time_delta = base::TimeTicks::Now() - start_tick;
  int64_t delta = time_delta.ToInternalValue();
  base::debug::Alias(&delta);
  UINT32 size = g_font_loader->GetFontMapSize();
  base::debug::Alias(&size);
  base::debug::Alias(&loading_restricted);

  CHECK(SUCCEEDED(hr));
  CHECK(font_collection.Get() != NULL);

  base::debug::ClearCrashKey(kFontKeyName);

  return true;
}

bool ValidateFontCacheFile(base::File* file) {
  DCHECK(file != NULL);
  CacheFileHeader file_header;
  if (file->Read(0, reinterpret_cast<char*>(&file_header), sizeof(file_header))
      == -1) {
    return false;
  }
  return ValidateFontCacheHeader(&file_header);
}

bool LoadFontCache(const base::FilePath& path) {
  scoped_ptr<base::File> file(new base::File(path,
                              base::File::FLAG_OPEN | base::File::FLAG_READ));
  if (!file->IsValid())
    return false;

  if (!ValidateFontCacheFile(file.get()))
    return false;

  base::string16 name(base::ASCIIToUTF16(content::kFontCacheSharedSectionName));
  name.append(base::UintToString16(base::GetCurrentProcId()));
  HANDLE mapping = ::CreateFileMapping(
      file->GetPlatformFile(),
      NULL,
      PAGE_READONLY,
      0,
      0,
      name.c_str());
  if (mapping == INVALID_HANDLE_VALUE)
    return false;

  if (::GetLastError() == ERROR_ALREADY_EXISTS) {
    CloseHandle(mapping);
    // We crash here, as no one should have created this mapping except Chrome.
    CHECK(false);
    return false;
  }

  DCHECK(!g_shared_font_cache.IsValid());
  g_shared_font_cache.Set(mapping);

  return true;
}

bool BuildFontCache(const base::FilePath& file) {
  return BuildFontCacheInternal(file.value().c_str());
}

bool ShouldUseDirectWriteFontProxyFieldTrial() {
  return base::StartsWith(
      base::FieldTrialList::FindFullName("DirectWriteFontProxy"),
      "UseDirectWriteFontProxy", base::CompareCase::SENSITIVE);
}

}  // namespace content
