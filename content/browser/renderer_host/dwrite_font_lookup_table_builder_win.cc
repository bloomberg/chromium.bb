// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/dwrite_font_lookup_table_builder_win.h"

#include <dwrite.h>
#include <dwrite_2.h>
#include <algorithm>
#include <set>
#include <utility>

#include "base/file_version_info.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "base/win/registry.h"
#include "content/browser/renderer_host/dwrite_font_file_util_win.h"
#include "content/browser/renderer_host/dwrite_font_uma_logging_win.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/font_unique_name_lookup/icu_fold_case_util.h"
#include "ui/gfx/win/direct_write.h"

namespace content {

using namespace dwrite_font_file_util;
using namespace dwrite_font_uma_logging;

namespace {

const base::FilePath::CharType kProtobufFilename[] =
    FILE_PATH_LITERAL("font_unique_name_table.pb");

// The unresponsive renderer timeout is 30 seconds (kDefaultCommitTimeout). As a
// starting point, let's set the max time for indexing fonts to half of that 15
// seconds. We're starting the font lookup table construction earlier at startup
// before a renderer requests this.  Once we have more UMA data, we can look
// into performance details and reducing this timeout. This timeout is meant to
// cover pathological cases of font indexing where a Windows installation has an
// unusually large collection of fonts. In practice, building the unique font
// name table should not take longer than tens of milliseconds (~26 ms on a
// developer machine, Windows 10, default fonts).
const base::TimeDelta kFontIndexingTimeout = base::TimeDelta::FromSeconds(15);

const base::TimeDelta kIndexingSlowDownForTesting =
    base::TimeDelta::FromMilliseconds(1200);

bool ExtractCaseFoldedLocalizedStrings(
    IDWriteLocalizedStrings* dwrite_localized_strings,
    std::vector<std::string>* localized_strings) {
  uint32_t strings_count = dwrite_localized_strings->GetCount();

  if (!strings_count)
    return false;

  localized_strings->reserve(localized_strings->size() + strings_count);
  for (UINT32 j = 0; j < strings_count; ++j) {
    UINT32 length;
    HRESULT hr = dwrite_localized_strings->GetStringLength(j, &length);
    if (FAILED(hr))
      continue;
    std::wstring localized_name;
    localized_name.resize(length + 1);
    hr = dwrite_localized_strings->GetString(j, &localized_name[0], length + 1);
    if (FAILED(hr)) {
      continue;
    }
    localized_name.resize(length);
    // The documentation for the API call does not specify an encoding but the
    // results are wchar_t and FireFox considers them UTF-16, as seen here:
    // https://dxr.mozilla.org/mozilla-central/source/gfx/thebes/gfxDWriteFontList.cpp#90
    // so we'll assume that.
    localized_strings->push_back(base::UTF16ToUTF8(
        base::i18n::FoldCase(base::string16(localized_name))));
  }
  return true;
}

// Used in the BuildFontUniqueNameTable() method to ensure that
// |font_table_built_| is signaled whenever the function exits due to an error
// or otherwise.
class ScopedAutoSignal {
 public:
  ScopedAutoSignal(base::WaitableEvent* waitable_event)
      : waitable_event_(waitable_event) {}
  ~ScopedAutoSignal() { waitable_event_->Signal(); }

 private:
  base::WaitableEvent* const waitable_event_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAutoSignal);
};

bool EnsureCacheDirectory(base::FilePath cache_directory) {
  // If the directory does not exist already, ensure that the parent directory
  // exists, which is usually the profile directory. If it exists, we can try
  // creating the cache directory.
  return base::DirectoryExists(cache_directory) ||
         (base::DirectoryExists(cache_directory.DirName()) &&
          CreateDirectory(cache_directory));
}

}  // namespace

DWriteFontLookupTableBuilder::FontFileWithUniqueNames::FontFileWithUniqueNames(
    blink::FontUniqueNameTable_UniqueFont&& font,
    std::vector<std::string>&& names)
    : font_entry(std::move(font)), extracted_names(std::move(names)) {}

DWriteFontLookupTableBuilder::FontFileWithUniqueNames::
    ~FontFileWithUniqueNames() = default;

DWriteFontLookupTableBuilder::FontFileWithUniqueNames::FontFileWithUniqueNames(
    DWriteFontLookupTableBuilder::FontFileWithUniqueNames&& other) = default;

DWriteFontLookupTableBuilder::DWriteFontLookupTableBuilder() {
  // In FontUniqueNameBrowserTest the DWriteFontLookupTableBuilder is
  // instantiated to configure the cache directory for testing explicitly before
  // GetContentClient() is available. Catch this case here. It is safe to not
  // set the cache directory here, as an invalid cache directory would be
  // detected by TableCacheFilePath and the LoadFromFile and PersistToFile
  // methods.
  cache_directory_ =
      GetContentClient()
          ? GetContentClient()->browser()->GetFontLookupTableCacheDir()
          : base::FilePath();
}

DWriteFontLookupTableBuilder::~DWriteFontLookupTableBuilder() = default;

base::ReadOnlySharedMemoryRegion
DWriteFontLookupTableBuilder::DuplicateMemoryRegion() {
  DCHECK(EnsureFontUniqueNameTable());
  return font_table_memory_.region.Duplicate();
}

bool DWriteFontLookupTableBuilder::IsFontUniqueNameTableValid() {
  return font_table_memory_.IsValid() && font_table_memory_.mapping.size();
}

void DWriteFontLookupTableBuilder::InitializeDirectWrite() {
  if (direct_write_initialized_)
    return;
  direct_write_initialized_ = true;

  Microsoft::WRL::ComPtr<IDWriteFactory> factory;
  gfx::win::CreateDWriteFactory(&factory);
  if (factory == nullptr) {
    // We won't be able to load fonts, but we should still return messages so
    // renderers don't hang if they for some reason send us a font message.
    return;
  }

  // QueryInterface for IDWriteFactory2. It's ok for this to fail if we are
  // running an older version of DirectWrite (earlier than Win8.1).
  factory.As<IDWriteFactory2>(&factory2_);

  HRESULT hr = factory->GetSystemFontCollection(&collection_);
  DCHECK(SUCCEEDED(hr));

  if (!collection_) {
    base::UmaHistogramSparse(
        "DirectWrite.Fonts.Proxy.GetSystemFontCollectionResult", hr);
    LogMessageFilterError(MessageFilterError::ERROR_NO_COLLECTION);
    return;
  }
}

std::string DWriteFontLookupTableBuilder::ComputePersistenceHash() {
  // Build a hash from DWrite product version, browser product version and font
  // names and file paths as stored in the registry. The browser product version
  // is included to ensure that the cache is rebuild at least once for every
  // Chrome release. DWrite DLL version is included to ensure that any change in
  // DWrite behavior after an update does not interfere with the information we
  // have in the cache. The font registry keys and values are used to detect
  // changes in installed fonts.

  std::unique_ptr<FileVersionInfo> dwrite_version_info =
      FileVersionInfo::CreateFileVersionInfo(
          base::FilePath(FILE_PATH_LITERAL("DWrite.dll")));

  DCHECK(dwrite_version_info);

  std::string product_version =
      base::WideToUTF8(dwrite_version_info->product_version());

  std::string to_hash = product_version;

  const wchar_t* kFonts =
      L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";
  base::win::RegistryValueIterator it(HKEY_LOCAL_MACHINE, kFonts);
  DCHECK(it.ValueCount());
  for (; it.Valid(); ++it) {
    to_hash.append(base::WideToUTF8(it.Name()));
    to_hash.append(base::WideToUTF8(it.Value()));
  }

  DCHECK(GetContentClient());
  to_hash.append(GetContentClient()->browser()->GetProduct());

  uint32_t fonts_changed_hash = base::PersistentHash(to_hash);
  return std::to_string(fonts_changed_hash);
}

void DWriteFontLookupTableBuilder::SetCacheDirectoryForTesting(
    base::FilePath cache_directory) {
  cache_directory_ = cache_directory;
}

void DWriteFontLookupTableBuilder::SetCachingEnabledForTesting(
    bool caching_enabled) {
  caching_enabled_ = caching_enabled;
}

base::FilePath DWriteFontLookupTableBuilder::TableCacheFilePath() {
  if (!EnsureCacheDirectory(cache_directory_)) {
    LOG(ERROR) << "Unable to determine cache file location for "
                  "DWriteFontLookupTableBuilder.";
    return base::FilePath();
  }
  return cache_directory_.Append(kProtobufFilename);
}

bool DWriteFontLookupTableBuilder::PersistToFile() {
  DCHECK(caching_enabled_);
  DCHECK(IsFontUniqueNameTableValid());

  if (!IsFontUniqueNameTableValid())
    return false;
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::File table_cache_file(
      TableCacheFilePath(),
      base::File::FLAG_CREATE_ALWAYS | base::File::Flags::FLAG_WRITE);
  if (!table_cache_file.IsValid())
    return false;
  if (table_cache_file.Write(
          0, static_cast<char*>(font_table_memory_.mapping.memory()),
          font_table_memory_.mapping.size()) == -1) {
    table_cache_file.SetLength(0);
    return false;
  }
  return true;
}

bool DWriteFontLookupTableBuilder::LoadFromFile() {
  DCHECK(caching_enabled_);
  // Reset to empty to ensure IsValid() is false if reading fails.
  font_table_memory_ = base::MappedReadOnlyRegion();
  base::File table_cache_file;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    table_cache_file =
        base::File(TableCacheFilePath(),
                   base::File::FLAG_OPEN | base::File::Flags::FLAG_READ);
    if (!table_cache_file.IsValid())
      return false;
  }
  font_table_memory_ =
      base::ReadOnlySharedMemoryRegion::Create(table_cache_file.GetLength());
  if (!IsFontUniqueNameTableValid())
    return false;
  int read_result = table_cache_file.Read(
      0, static_cast<char*>(font_table_memory_.mapping.memory()),
      table_cache_file.GetLength());
  // If no bytes were read or Read() returned -1 we are not able to reconstruct
  // a font table from the cached file.
  if (read_result <= 0) {
    font_table_memory_ = base::MappedReadOnlyRegion();
    return false;
  }

  blink::FontUniqueNameTable font_table;
  if (!font_table.ParseFromArray(font_table_memory_.mapping.memory(),
                                 font_table_memory_.mapping.size())) {
    // TODO(https://crbug.com/941434): Track failure to parse cache in UMA data.
    font_table_memory_ = base::MappedReadOnlyRegion();
    return false;
  }

  return true;
}

bool DWriteFontLookupTableBuilder::EnsureFontUniqueNameTable() {
  TRACE_EVENT0("dwrite,fonts",
               "DWriteFontLookupTableBuilder::EnsureFontUniqueNameTable");
  DCHECK(base::FeatureList::IsEnabled(features::kFontSrcLocalMatching));
  base::ScopedAllowBaseSyncPrimitives allow_base_sync_primitives;
  font_table_built_.Wait();
  return IsFontUniqueNameTableValid();
}

void DWriteFontLookupTableBuilder::SchedulePrepareFontUniqueNameTable() {
  DCHECK(base::FeatureList::IsEnabled(features::kFontSrcLocalMatching));

  // TODO(https://crbug.com/931366): Downgrade the priority of this startup
  // task once the UpdatePriority() API for sequenced task runners is in
  // place. Then bump the priority when the renderer needs the table to be
  // ready.
  scoped_refptr<base::SequencedTaskRunner> results_collection_task_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  results_collection_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&DWriteFontLookupTableBuilder::PrepareFontUniqueNameTable,
                     base::Unretained(this)));
}

void DWriteFontLookupTableBuilder::PrepareFontUniqueNameTable() {
  TRACE_EVENT0("dwrite,fonts",
               "DWriteFontLookupTableBuilder::BuildFontUniqueNameTable");
  // The table must only be built once.
  DCHECK(!font_table_built_.IsSignaled());

  if (caching_enabled_ && LoadFromFile()) {
    blink::FontUniqueNameTable font_table;
    bool update_needed =
        !IsFontUniqueNameTableValid() ||
        !font_table.ParseFromArray(font_table_memory_.mapping.memory(),
                                   font_table_memory_.mapping.size()) ||
        font_table.stored_for_platform_version_identifier() !=
            ComputePersistenceHash();

    if (!update_needed) {
      // TODO(https://crbug.com/941434): Add UMA to track successful read from
      // disk cache.
      font_table_built_.Signal();
      return;
    }
  }

  // TODO(https://crbug.com/941434): Add UMA to track required cache rebuild.

  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    InitializeDirectWrite();
  }

  start_time_ = base::TimeTicks::Now();
  font_unique_name_table_ = std::make_unique<blink::FontUniqueNameTable>();

  // The |stored_for_platform_version_identifier| proto field is used for
  // persisting the table to disk and identifying whether an update to the
  // table is needed when loading it back.
  font_unique_name_table_->set_stored_for_platform_version_identifier(
      ComputePersistenceHash());

  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    outstanding_family_results_ = collection_->GetFontFamilyCount();
  }
  for (UINT32 family_index = 0; family_index < outstanding_family_results_;
       ++family_index) {
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(
            &DWriteFontLookupTableBuilder::ExtractPathAndNamesFromFamily,
            collection_, family_index, start_time_, slow_down_mode_for_testing_,
            OptionalOrNullptr(hang_event_for_testing_)),
        base::BindOnce(&DWriteFontLookupTableBuilder::
                           AppendFamilyResultAndFinalizeIfNeeded,
                       base::Unretained(this)));
  }
  // Post a task to catch timeouts should one of the
  // tasks will eventually not reply.
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DWriteFontLookupTableBuilder::OnTimeout,
                     base::Unretained(this)),
      kFontIndexingTimeout);
}

// static
DWriteFontLookupTableBuilder::FamilyResult
DWriteFontLookupTableBuilder::ExtractPathAndNamesFromFamily(
    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection,
    uint32_t family_index,
    base::TimeTicks start_time,
    SlowDownMode slow_down_mode_for_testing,
    base::WaitableEvent* hang_event_for_testing) {
  TRACE_EVENT0("dwrite,fonts",
               "DWriteFontLookupTableBuilder::ExtractPathAndNamesFromFamily");

  static base::NoDestructor<base::string16> windows_fonts_path(
      GetWindowsFontsPath());

  DWriteFontLookupTableBuilder::FamilyResult family_result;

  if (base::TimeTicks::Now() - start_time > kFontIndexingTimeout)
    return family_result;

  Microsoft::WRL::ComPtr<IDWriteFontFamily> family;
  HRESULT hr = collection->GetFontFamily(family_index, &family);
  if (FAILED(hr)) {
    return family_result;
  }
  UINT32 font_count = family->GetFontCount();

  for (UINT32 font_index = 0; font_index < font_count; ++font_index) {
    Microsoft::WRL::ComPtr<IDWriteFont> font;
    {
      base::ScopedBlockingCall scoped_blocking_call(
          FROM_HERE, base::BlockingType::MAY_BLOCK);
      hr = family->GetFont(font_index, &font);
    }
    if (FAILED(hr))
      return family_result;

    if (font->GetSimulations() != DWRITE_FONT_SIMULATIONS_NONE)
      continue;

    std::set<base::string16> path_set;
    std::set<base::string16> custom_font_path_set;
    uint32_t ttc_index = 0;
    {
      base::ScopedBlockingCall scoped_blocking_call(
          FROM_HERE, base::BlockingType::MAY_BLOCK);
      if (!AddFilesForFont(font.Get(), *windows_fonts_path, &path_set,
                           &custom_font_path_set, &ttc_index)) {
        // It's possible to not be able to retrieve a font file for a font that
        // is in the system font collection, see https://crbug.com/922183. If we
        // were not able to retrieve a file for a registered font, we do not
        // need to add it to the map.
        continue;
      }
    }

    // After having received clarification from Microsoft, the API is designed
    // for allowing multiple files to be returned, if MS was to support a file
    // format like Type1 fonts with this API, but for now only ever returns 1
    // font file as only TrueType / OpenType fonts are supported.
    DCHECK_EQ(path_set.size() + custom_font_path_set.size(), 1u);
    // If this font is placed in a custom font path location, we pass it to
    // Blink, and we'll track with UMA there if such a font path is matched
    // and used. If this happens more than very rarely, we will need to add an
    // out-of-process loading mechanism for loading those uniquely matched
    // font files.
    base::FilePath file_path(path_set.size() ? *path_set.begin()
                                             : *custom_font_path_set.begin());
    DCHECK(!file_path.empty());

    // Build entry for being added to the table in separate call.
    blink::FontUniqueNameTable_UniqueFont unique_font;
    unique_font.set_file_path(file_path.AsUTF8Unsafe());
    unique_font.set_ttc_index(ttc_index);

    std::vector<std::string> extracted_names;
    auto extract_names =
        [&extracted_names, &hr,
         &font](DWRITE_INFORMATIONAL_STRING_ID font_info_string_id) {
          // Now get names, and make them point to the added font.
          IDWriteLocalizedStrings* font_id_keyed_names;
          BOOL has_id_keyed_names;
          {
            base::ScopedBlockingCall scoped_blocking_call(
                FROM_HERE, base::BlockingType::MAY_BLOCK);
            hr = font->GetInformationalStrings(
                font_info_string_id, &font_id_keyed_names, &has_id_keyed_names);
            if (FAILED(hr) || !has_id_keyed_names)
              return;
          }

          ExtractCaseFoldedLocalizedStrings(font_id_keyed_names,
                                            &extracted_names);
        };

    extract_names(DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME);
    extract_names(DWRITE_INFORMATIONAL_STRING_FULL_NAME);

    if (UNLIKELY(slow_down_mode_for_testing == SlowDownMode::kDelayEachTask))
      base::PlatformThread::Sleep(kIndexingSlowDownForTesting);
    else if (UNLIKELY(slow_down_mode_for_testing ==
                      SlowDownMode::kHangOneTask) &&
             family_index == 0) {
      base::ScopedAllowBaseSyncPrimitivesForTesting scoped_allow_sync_;
      DCHECK(hang_event_for_testing);
      hang_event_for_testing->Wait();
    }

    if (!extracted_names.size())
      continue;

    family_result.push_back(
        DWriteFontLookupTableBuilder::FontFileWithUniqueNames(
            std::move(unique_font), std::move(extracted_names)));
  }

  return family_result;
}

void DWriteFontLookupTableBuilder::AppendFamilyResultAndFinalizeIfNeeded(
    const FamilyResult& family_result) {
  TRACE_EVENT0(
      "dwrite,fonts",
      "DWriteFontLookupTableBuilder::AppendFamilyResultAndFinalizeIfNeeded");

  outstanding_family_results_--;

  // If this task's response came late and OnTimeout was called, we
  // do not need the results anymore and the table was already finalized.
  if (font_table_built_.IsSignaled())
    return;

  for (const FontFileWithUniqueNames& font_of_family : family_result) {
    blink::FontUniqueNameTable_UniqueFont* added_unique_font =
        font_unique_name_table_->add_fonts();

    *added_unique_font = font_of_family.font_entry;

    int added_font_index = font_unique_name_table_->fonts_size() - 1;

    for (auto& font_name : font_of_family.extracted_names) {
      blink::FontUniqueNameTable_UniqueNameToFontMapping* added_mapping =
          font_unique_name_table_->add_name_map();
      DCHECK(added_mapping);
      added_mapping->set_font_name(font_name);
      added_mapping->set_font_index(added_font_index);
    }
  }

  if (!outstanding_family_results_) {
    FinalizeFontTable();
  }
}

void DWriteFontLookupTableBuilder::FinalizeFontTable() {
  TRACE_EVENT0("dwrite,fonts",
               "DWriteFontLookupTableBuilder::FinalizeFontTable");
  DCHECK(!font_table_built_.IsSignaled());
  ScopedAutoSignal auto_signal(&font_table_built_);

  if (base::TimeTicks::Now() - start_time_ > kFontIndexingTimeout) {
    LOG(ERROR) << "Creating unique font lookup table timed out, emptying "
                  "partial table.";
    font_unique_name_table_->clear_fonts();
    font_unique_name_table_->clear_name_map();
  }

  // Make sure that whatever happens in the remainder of this function the
  // FontUniqueNameTable object gets released by moving it to a local variable.
  std::unique_ptr<blink::FontUniqueNameTable> font_unique_name_table(
      std::move(font_unique_name_table_));

  unsigned num_font_files = font_unique_name_table->fonts_size();

  // Sort names for using binary search on this proto in FontTableMatcher.
  std::sort(font_unique_name_table->mutable_name_map()->begin(),
            font_unique_name_table->mutable_name_map()->end(),
            [](const blink::FontUniqueNameTable_UniqueNameToFontMapping& a,
               const blink::FontUniqueNameTable_UniqueNameToFontMapping& b) {
              return a.font_name() < b.font_name();
            });

  font_table_memory_ = base::ReadOnlySharedMemoryRegion::Create(
      font_unique_name_table->ByteSizeLong());
  if (!IsFontUniqueNameTableValid()) {
    return;
  }

  if (!font_unique_name_table->SerializeToArray(
          font_table_memory_.mapping.memory(),
          font_table_memory_.mapping.size())) {
    font_table_memory_ = base::MappedReadOnlyRegion();
    return;
  }

  if (caching_enabled_) {
    PersistToFile();
    // TODO(https://crbug.com/941434): Track failure to persist cache in UMA.
  }

  base::TimeDelta duration = base::TimeTicks::Now() - start_time_;
  UMA_HISTOGRAM_MEDIUM_TIMES("DirectWrite.Fonts.Proxy.LookupTableBuildTime",
                             duration);
  // The size is usually tens of kilobytes, ~50kb on a standard Windows 10
  // installation, 1MB should be a more than high enough upper limit.
  UMA_HISTOGRAM_CUSTOM_COUNTS("DirectWrite.Fonts.Proxy.LookupTableSize",
                              font_table_memory_.mapping.size() / 1024, 1, 1000,
                              50);

  UMA_HISTOGRAM_CUSTOM_COUNTS("DirectWrite.Fonts.Proxy.NumFontFiles",
                              num_font_files, 1, 5000, 50);

  UMA_HISTOGRAM_CUSTOM_COUNTS("DirectWrite.Fonts.Proxy.IndexingSpeed",
                              num_font_files / duration.InSecondsF(), 1, 10000,
                              50);
}

void DWriteFontLookupTableBuilder::OnTimeout() {
  if (!font_table_built_.IsSignaled())
    FinalizeFontTable();
}

void DWriteFontLookupTableBuilder::SetSlowDownIndexingForTesting(
    SlowDownMode slow_down_mode) {
  slow_down_mode_for_testing_ = slow_down_mode;
  if (slow_down_mode == SlowDownMode::kHangOneTask)
    hang_event_for_testing_.emplace();
}

void DWriteFontLookupTableBuilder::ResetLookupTableForTesting() {
  slow_down_mode_for_testing_ = SlowDownMode::kNoSlowdown;
  font_table_memory_ = base::MappedReadOnlyRegion();
  caching_enabled_ = true;
  font_table_built_.Reset();
}

void DWriteFontLookupTableBuilder::ResumeFromHangForTesting() {
  hang_event_for_testing_->Signal();
}

// static
DWriteFontLookupTableBuilder* DWriteFontLookupTableBuilder::GetInstance() {
  static base::NoDestructor<DWriteFontLookupTableBuilder> instance;
  return instance.get();
}

}  // namespace content
