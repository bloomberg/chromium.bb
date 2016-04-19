// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/dwrite_font_proxy/dwrite_font_proxy_win.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "content/child/dwrite_font_proxy/dwrite_localized_strings_win.h"
#include "content/common/dwrite_font_proxy_messages.h"
#include "content/public/child/child_thread.h"
#include "ipc/ipc_sender.h"

namespace mswr = Microsoft::WRL;

namespace content {

namespace {

// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration.
enum DirectWriteLoadFamilyResult {
  LOAD_FAMILY_SUCCESS_SINGLE_FAMILY = 0,
  LOAD_FAMILY_SUCCESS_MATCHED_FAMILY = 1,
  LOAD_FAMILY_ERROR_MULTIPLE_FAMILIES = 2,
  LOAD_FAMILY_ERROR_NO_FAMILIES = 3,
  LOAD_FAMILY_ERROR_NO_COLLECTION = 4,

  LOAD_FAMILY_MAX_VALUE
};

const char kFontKeyName[] = "font_key_name";

const base::Feature kFileLoadingExperimentFeature{
    "DirectWriteFontFileLoadingExperiment", base::FEATURE_DISABLED_BY_DEFAULT};

void LogLoadFamilyResult(DirectWriteLoadFamilyResult result) {
  UMA_HISTOGRAM_ENUMERATION("DirectWrite.Fonts.Proxy.LoadFamilyResult", result,
                            LOAD_FAMILY_MAX_VALUE);
}

}  // namespace

DWriteFontCollectionProxy::DWriteFontCollectionProxy() = default;

DWriteFontCollectionProxy::~DWriteFontCollectionProxy() = default;

HRESULT DWriteFontCollectionProxy::FindFamilyName(const WCHAR* family_name,
                                                  UINT32* index,
                                                  BOOL* exists) {
  DCHECK(family_name);
  DCHECK(index);
  DCHECK(exists);
  TRACE_EVENT0("dwrite", "FontProxy::FindFamilyName");

  uint32_t family_index = 0;
  base::string16 name(family_name);

  auto iter = family_names_.find(name);
  if (iter != family_names_.end()) {
    *index = iter->second;
    *exists = iter->second != UINT_MAX;
    return S_OK;
  }

  if (!GetSender()->Send(
          new DWriteFontProxyMsg_FindFamily(name, &family_index))) {
    return E_FAIL;
  }

  if (family_index != UINT32_MAX) {
    if (!CreateFamily(family_index))
      return E_FAIL;
    *exists = TRUE;
    *index = family_index;
    families_[family_index]->SetName(name);
  } else {
    *exists = FALSE;
    *index = UINT32_MAX;
  }

  family_names_[name] = *index;
  return S_OK;
}

HRESULT DWriteFontCollectionProxy::GetFontFamily(
    UINT32 index,
    IDWriteFontFamily** font_family) {
  DCHECK(font_family);

  if (index < families_.size() && families_[index]) {
    families_[index].CopyTo(font_family);
    return S_OK;
  }

  if (!CreateFamily(index))
    return E_FAIL;

  families_[index].CopyTo(font_family);
  return S_OK;
}

UINT32 DWriteFontCollectionProxy::GetFontFamilyCount() {
  if (family_count_ != UINT_MAX)
    return family_count_;

  TRACE_EVENT0("dwrite", "FontProxy::GetFontFamilyCount");

  uint32_t family_count = 0;
  if (!GetSender()->Send(
          new DWriteFontProxyMsg_GetFamilyCount(&family_count))) {
    return 0;
  }
  family_count_ = family_count;
  return family_count;
}

HRESULT DWriteFontCollectionProxy::GetFontFromFontFace(
    IDWriteFontFace* font_face,
    IDWriteFont** font) {
  DCHECK(font_face);
  DCHECK(font);

  for (const auto& family : families_) {
    if (family && family->GetFontFromFontFace(font_face, font)) {
      return S_OK;
    }
  }
  // If the font came from our collection, at least one family should match
  DCHECK(false);

  return E_FAIL;
}

HRESULT DWriteFontCollectionProxy::CreateEnumeratorFromKey(
    IDWriteFactory* factory,
    const void* collection_key,
    UINT32 collection_key_size,
    IDWriteFontFileEnumerator** font_file_enumerator) {
  if (!collection_key || collection_key_size != sizeof(uint32_t)) {
    return E_INVALIDARG;
  }

  TRACE_EVENT0("dwrite", "FontProxy::LoadingFontFiles");

  const uint32_t* family_index =
      reinterpret_cast<const uint32_t*>(collection_key);

  if (*family_index >= GetFontFamilyCount()) {
    return E_INVALIDARG;
  }

  // If we already loaded the family we should reuse the existing collection.
  DCHECK(!families_[*family_index]->IsLoaded());

  std::vector<base::string16> file_names;
  if (!GetSender()->Send(
          new DWriteFontProxyMsg_GetFontFiles(*family_index, &file_names))) {
    return E_FAIL;
  }

  HRESULT hr = mswr::MakeAndInitialize<FontFileEnumerator>(
      font_file_enumerator, factory, this, &file_names);

  if (!SUCCEEDED(hr)) {
    DCHECK(false);
    return E_FAIL;
  }

  return S_OK;
}

HRESULT DWriteFontCollectionProxy::CreateStreamFromKey(
    const void* font_file_reference_key,
    UINT32 font_file_reference_key_size,
    IDWriteFontFileStream** font_file_stream) {
  if (!font_file_reference_key) {
    return E_FAIL;
  }

  const base::char16* file_name =
      reinterpret_cast<const base::char16*>(font_file_reference_key);
  DCHECK_EQ(font_file_reference_key_size % sizeof(base::char16), 0u);
  size_t file_name_size =
      static_cast<size_t>(font_file_reference_key_size) / sizeof(base::char16);

  if (file_name_size == 0 || file_name[file_name_size - 1] != L'\0') {
    return E_FAIL;
  }

  TRACE_EVENT0("dwrite", "FontFileEnumerator::CreateStreamFromKey");

  mswr::ComPtr<IDWriteFontFileStream> stream;
  if (!SUCCEEDED(mswr::MakeAndInitialize<FontFileStream>(&stream, file_name))) {
    DCHECK(false);
    return E_FAIL;
  }
  *font_file_stream = stream.Detach();
  return S_OK;
}

HRESULT DWriteFontCollectionProxy::RuntimeClassInitialize(
    IDWriteFactory* factory,
    IPC::Sender* sender_override) {
  DCHECK(factory);

  factory_ = factory;
  sender_override_ = sender_override;

  HRESULT hr = factory->RegisterFontCollectionLoader(this);
  DCHECK(SUCCEEDED(hr));
  hr = factory_->RegisterFontFileLoader(this);
  DCHECK(SUCCEEDED(hr));
  return S_OK;
}

void DWriteFontCollectionProxy::Unregister() {
  factory_->UnregisterFontCollectionLoader(this);
  factory_->UnregisterFontFileLoader(this);
}

bool DWriteFontCollectionProxy::LoadFamily(
    UINT32 family_index,
    IDWriteFontCollection** containing_collection) {
  TRACE_EVENT0("dwrite", "FontProxy::LoadFamily");

  uint32_t index = family_index;
  // CreateCustomFontCollection ends up calling
  // DWriteFontCollectionProxy::CreateEnumeratorFromKey.
  HRESULT hr = factory_->CreateCustomFontCollection(
      this /*collectionLoader*/, reinterpret_cast<const void*>(&index),
      sizeof(index), containing_collection);

  return SUCCEEDED(hr);
}

bool DWriteFontCollectionProxy::GetFontFamily(UINT32 family_index,
                                              const base::string16& family_name,
                                              IDWriteFontFamily** font_family) {
  DCHECK(font_family);
  DCHECK(!family_name.empty());
  if (!CreateFamily(family_index))
    return false;

  mswr::ComPtr<DWriteFontFamilyProxy>& family = families_[family_index];
  if (!family->IsLoaded() || family->GetName().empty())
    family->SetName(family_name);

  family.CopyTo(font_family);
  return true;
}

bool DWriteFontCollectionProxy::LoadFamilyNames(
    UINT32 family_index,
    IDWriteLocalizedStrings** localized_strings) {
  TRACE_EVENT0("dwrite", "FontProxy::LoadFamilyNames");

  std::vector<std::pair<base::string16, base::string16>> strings;
  if (!GetSender()->Send(
          new DWriteFontProxyMsg_GetFamilyNames(family_index, &strings))) {
    return false;
  }

  HRESULT hr = mswr::MakeAndInitialize<DWriteLocalizedStrings>(
      localized_strings, &strings);

  return SUCCEEDED(hr);
}

bool DWriteFontCollectionProxy::CreateFamily(UINT32 family_index) {
  if (family_index < families_.size() && families_[family_index])
    return true;

  UINT32 family_count = GetFontFamilyCount();
  if (family_index >= family_count) {
    return false;
  }

  if (families_.size() < family_count)
    families_.resize(family_count);

  mswr::ComPtr<DWriteFontFamilyProxy> family;
  HRESULT hr = mswr::MakeAndInitialize<DWriteFontFamilyProxy>(&family, this,
                                                              family_index);
  DCHECK(SUCCEEDED(hr));
  DCHECK_LT(family_index, families_.size());

  families_[family_index] = family;
  return true;
}

IPC::Sender* DWriteFontCollectionProxy::GetSender() {
  return sender_override_ ? sender_override_ : ChildThread::Get();
}

DWriteFontFamilyProxy::DWriteFontFamilyProxy() = default;

DWriteFontFamilyProxy::~DWriteFontFamilyProxy() = default;

HRESULT DWriteFontFamilyProxy::GetFontCollection(
    IDWriteFontCollection** font_collection) {
  DCHECK(font_collection);

  proxy_collection_.CopyTo(font_collection);
  return S_OK;
}

UINT32 DWriteFontFamilyProxy::GetFontCount() {
  // We could conceivably proxy just the font count. However, calling
  // GetFontCount is almost certain to be followed by a series of GetFont
  // calls which will need to load all the fonts anyway, so we might as
  // well save an IPC here.
  if (!LoadFamily())
    return 0;

  return family_->GetFontCount();
}

HRESULT DWriteFontFamilyProxy::GetFont(UINT32 index, IDWriteFont** font) {
  DCHECK(font);

  if (index >= GetFontCount()) {
    return E_INVALIDARG;
  }
  if (!LoadFamily())
    return E_FAIL;

  return family_->GetFont(index, font);
}

HRESULT DWriteFontFamilyProxy::GetFamilyNames(IDWriteLocalizedStrings** names) {
  DCHECK(names);

  // Prefer the real thing, if available.
  if (family_) {
    family_names_.Reset();  // Release cached data.
    return family_->GetFamilyNames(names);
  }

  // If already cached, use the cache.
  if (family_names_) {
    family_names_.CopyTo(names);
    return S_OK;
  }

  TRACE_EVENT0("dwrite", "FontProxy::GetFamilyNames");

  // Otherwise, do the IPC.
  if (!proxy_collection_->LoadFamilyNames(family_index_, &family_names_))
    return E_FAIL;

  family_names_.CopyTo(names);
  return S_OK;
}

HRESULT DWriteFontFamilyProxy::GetFirstMatchingFont(
    DWRITE_FONT_WEIGHT weight,
    DWRITE_FONT_STRETCH stretch,
    DWRITE_FONT_STYLE style,
    IDWriteFont** matching_font) {
  DCHECK(matching_font);

  if (!LoadFamily())
    return E_FAIL;

  return family_->GetFirstMatchingFont(weight, stretch, style, matching_font);
}

HRESULT DWriteFontFamilyProxy::GetMatchingFonts(
    DWRITE_FONT_WEIGHT weight,
    DWRITE_FONT_STRETCH stretch,
    DWRITE_FONT_STYLE style,
    IDWriteFontList** matching_fonts) {
  DCHECK(matching_fonts);

  if (!LoadFamily())
    return E_FAIL;

  return family_->GetMatchingFonts(weight, stretch, style, matching_fonts);
}

HRESULT DWriteFontFamilyProxy::RuntimeClassInitialize(
    DWriteFontCollectionProxy* collection,
    UINT32 index) {
  DCHECK(collection);

  proxy_collection_ = collection;
  family_index_ = index;
  return S_OK;
}

bool DWriteFontFamilyProxy::GetFontFromFontFace(IDWriteFontFace* font_face,
                                                IDWriteFont** font) {
  DCHECK(font_face);
  DCHECK(font);

  if (!family_)
    return false;

  mswr::ComPtr<IDWriteFontCollection> collection;
  HRESULT hr = family_->GetFontCollection(&collection);
  DCHECK(SUCCEEDED(hr));
  hr = collection->GetFontFromFontFace(font_face, font);

  return SUCCEEDED(hr);
}

void DWriteFontFamilyProxy::SetName(const base::string16& family_name) {
  family_name_.assign(family_name);
}

const base::string16& DWriteFontFamilyProxy::GetName() {
  return family_name_;
}

bool DWriteFontFamilyProxy::IsLoaded() {
  return family_ != nullptr;
}

bool DWriteFontFamilyProxy::LoadFamily() {
  if (family_)
    return true;

  SCOPED_UMA_HISTOGRAM_TIMER("DirectWrite.Fonts.Proxy.LoadFamilyTime");

  base::debug::ScopedCrashKey crash_key(kFontKeyName,
                                        base::WideToUTF8(family_name_));

  mswr::ComPtr<IDWriteFontCollection> collection;
  if (!proxy_collection_->LoadFamily(family_index_, &collection)) {
    LogLoadFamilyResult(LOAD_FAMILY_ERROR_NO_COLLECTION);
    return false;
  }

  UINT32 family_count = collection->GetFontFamilyCount();

  HRESULT hr;
  if (family_count > 1) {
    // Some fonts are packaged in a single file containing multiple families. In
    // such a case we can find the right family by family name.
    DCHECK(!family_name_.empty());
    UINT32 family_index = 0;
    BOOL found = FALSE;
    hr =
        collection->FindFamilyName(family_name_.c_str(), &family_index, &found);
    if (SUCCEEDED(hr) && found) {
      hr = collection->GetFontFamily(family_index, &family_);
      LogLoadFamilyResult(LOAD_FAMILY_SUCCESS_MATCHED_FAMILY);
      return SUCCEEDED(hr);
    }
  }

  DCHECK_LE(family_count, 1u);

  if (family_count == 0) {
    // This is really strange, we successfully loaded no fonts?!
    LogLoadFamilyResult(LOAD_FAMILY_ERROR_NO_FAMILIES);
    return false;
  }

  LogLoadFamilyResult(family_count == 1 ? LOAD_FAMILY_SUCCESS_SINGLE_FAMILY
                                        : LOAD_FAMILY_ERROR_MULTIPLE_FAMILIES);

  hr = collection->GetFontFamily(0, &family_);

  return SUCCEEDED(hr);
}

FontFileEnumerator::FontFileEnumerator() = default;

FontFileEnumerator::~FontFileEnumerator() = default;

HRESULT FontFileEnumerator::GetCurrentFontFile(IDWriteFontFile** file) {
  DCHECK(file);
  if (current_file_ >= file_names_.size()) {
    return E_FAIL;
  }

  if (base::FeatureList::IsEnabled(kFileLoadingExperimentFeature)) {
    TRACE_EVENT0("dwrite",
                 "FontFileEnumerator::GetCurrentFontFile (directwrite)");
    HRESULT hr = factory_->CreateFontFileReference(
        file_names_[current_file_].c_str(), nullptr /* lastWriteTime*/, file);
    DCHECK(SUCCEEDED(hr));
    return hr;
  }

  TRACE_EVENT0("dwrite", "FontFileEnumerator::GetCurrentFontFile (memmap)");
  // CreateCustomFontFileReference ends up calling
  // DWriteFontCollectionProxy::CreateStreamFromKey.
  HRESULT hr = factory_->CreateCustomFontFileReference(
      reinterpret_cast<const void*>(file_names_[current_file_].c_str()),
      (file_names_[current_file_].length() + 1) * sizeof(base::char16),
      loader_.Get() /*IDWriteFontFileLoader*/, file);
  DCHECK(SUCCEEDED(hr));
  return hr;
}

HRESULT FontFileEnumerator::MoveNext(BOOL* has_current_file) {
  DCHECK(has_current_file);

  TRACE_EVENT0("dwrite", "FontFileEnumerator::MoveNext");
  if (next_file_ >= file_names_.size()) {
    *has_current_file = FALSE;
    current_file_ = UINT_MAX;
    return S_OK;
  }

  current_file_ = next_file_;
  ++next_file_;
  *has_current_file = TRUE;
  return S_OK;
}

HRESULT FontFileEnumerator::RuntimeClassInitialize(
    IDWriteFactory* factory,
    IDWriteFontFileLoader* loader,
    std::vector<base::string16>* file_names) {
  factory_ = factory;
  loader_ = loader;
  file_names_.swap(*file_names);
  file_streams_.resize(file_names_.size());
  return S_OK;
}

FontFileStream::FontFileStream() = default;

FontFileStream::~FontFileStream() = default;

HRESULT FontFileStream::GetFileSize(UINT64* file_size) {
  *file_size = data_.length();
  return S_OK;
}

HRESULT FontFileStream::GetLastWriteTime(UINT64* last_write_time) {
  *last_write_time = 0;
  return S_OK;
}

HRESULT FontFileStream::ReadFileFragment(const void** fragment_start,
                                         UINT64 fragment_offset,
                                         UINT64 fragment_size,
                                         void** fragment_context) {
  if (fragment_offset + fragment_size < fragment_offset)
    return E_FAIL;
  if (fragment_offset + fragment_size > data_.length())
    return E_FAIL;
  *fragment_start = data_.data() + fragment_offset;
  *fragment_context = nullptr;
  return S_OK;
}

HRESULT FontFileStream::RuntimeClassInitialize(
    const base::string16& file_name) {
  data_.Initialize(base::FilePath(file_name));
  if (!data_.IsValid())
    return E_FAIL;
  return S_OK;
}

}  // namespace content
