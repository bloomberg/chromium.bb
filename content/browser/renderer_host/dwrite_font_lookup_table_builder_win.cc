// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/dwrite_font_lookup_table_builder_win.h"

#include <dwrite.h>
#include <dwrite_2.h>
#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/dwrite_font_file_util_win.h"
#include "content/browser/renderer_host/dwrite_font_uma_logging_win.h"
#include "third_party/blink/public/common/font_unique_name_lookup/font_unique_name_table.pb.h"
#include "third_party/blink/public/common/font_unique_name_lookup/icu_fold_case_util.h"
#include "ui/gfx/win/direct_write.h"

namespace content {

using namespace dwrite_font_file_util;
using namespace dwrite_font_uma_logging;

namespace {
// The unresponsive renderer timeout is 30 seconds (kDefaultCommitTimeout). As a
// starting point, let's set the max time for indexing fonts to a third of that,
// 10 seconds and record a UMA histogram for how long indexing usually
// takes. Once we have UMA data, we can look into reducing this timeout. This
// timeout is meant to cover pathological cases of font indexing where a Windows
// installation has an unusually large collection of fonts. In practice,
// building the unique font name table should not take longer than tens of
// milliseconds (~26 ms on a developer machine, Windows 10, default fonts).
const base::TimeDelta kFontIndexingTimeout = base::TimeDelta::FromSeconds(10);

const base::TimeDelta kIndexingSlowDownForTesting =
    base::TimeDelta::FromMilliseconds(1200);

bool ExtractCaseFoldedLocalizedStrings(
    IDWriteLocalizedStrings* dwrite_localized_strings,
    std::vector<std::string>* localized_strings) {
  if (!dwrite_localized_strings->GetCount())
    return false;

  localized_strings->clear();
  localized_strings->reserve(dwrite_localized_strings->GetCount());
  for (UINT32 j = 0; j < dwrite_localized_strings->GetCount(); ++j) {
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

}  // namespace

DWriteFontLookupTableBuilder::DWriteFontLookupTableBuilder() = default;

DWriteFontLookupTableBuilder::~DWriteFontLookupTableBuilder() = default;

base::ReadOnlySharedMemoryRegion
DWriteFontLookupTableBuilder::DuplicatedMemoryRegion() {
  CHECK(EnsureFontUniqueNameTable());
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

bool DWriteFontLookupTableBuilder::EnsureFontUniqueNameTable() {
  if (IsFontUniqueNameTableValid())
    return true;

  BuildFontUniqueNameTable();
  return IsFontUniqueNameTableValid();
}

void DWriteFontLookupTableBuilder::BuildFontUniqueNameTable() {
  InitializeDirectWrite();

  base::TimeTicks time_ticks = base::TimeTicks::Now();
  blink::FontUniqueNameTable font_unique_name_table;
  bool timed_out = false;

  // The stored_for_platform_version_identifier proto field is used for
  // persisting the table to disk and identifiying whether and update to the
  // table is needed when loading it back. This functionality is not used on
  // Windows, hence setting it to the empty string is sufficient.
  font_unique_name_table.set_stored_for_platform_version_identifier("");

  base::string16 windows_fonts_path = GetWindowsFontsPath();

  for (UINT32 family_index = 0;
       family_index < collection_->GetFontFamilyCount(); ++family_index) {
    if (base::TimeTicks::Now() - time_ticks > kFontIndexingTimeout) {
      timed_out = true;
      break;
    }

    Microsoft::WRL::ComPtr<IDWriteFontFamily> family;
    HRESULT hr = collection_->GetFontFamily(family_index, &family);
    if (FAILED(hr)) {
      return;
    }
    UINT32 font_count = family->GetFontCount();

    for (UINT32 font_index = 0; font_index < font_count; ++font_index) {
      Microsoft::WRL::ComPtr<IDWriteFont> font;
      hr = family->GetFont(font_index, &font);
      if (FAILED(hr))
        return;

      if (font->GetSimulations() != DWRITE_FONT_SIMULATIONS_NONE)
        continue;

      std::set<base::string16> path_set;
      std::set<base::string16> custom_font_path_set;
      uint32_t ttc_index = 0;
      if (!AddFilesForFont(font.Get(), windows_fonts_path, &path_set,
                           &custom_font_path_set, &ttc_index)) {
        // It's possible to not be able to retrieve a font file for a font that
        // is in the system font collection, see https://crbug.com/922183. If we
        // were not able to retrieve a file for a registered font, we do not
        // need to add it to the map.
        continue;
      }

      // After having received clarification from Microsoft, the API is designed
      // for allowing multiple files to be returned, if MS was to support a file
      // format like Type1 fonts with this API, but for now only ever returns 1
      // font file as only TrueType / OpenType fonts are supported.
      CHECK_EQ(path_set.size() + custom_font_path_set.size(), 1u);
      // If this font is placed in a custom font path location, we pass it to
      // Blink, and we'll track with UMA there if such a font path is matched
      // and used. If this happens more than very rarely, we will need to add an
      // out-of-process loading mechanism for loading those uniquely matched
      // font files.
      base::FilePath file_path(path_set.size() ? *path_set.begin()
                                               : *custom_font_path_set.begin());
      CHECK(!file_path.empty());

      // Add file entry to map.
      blink::FontUniqueNameTable_UniqueFont* added_unique_font =
          font_unique_name_table.add_fonts();
      added_unique_font->set_file_path(file_path.AsUTF8Unsafe());
      added_unique_font->set_ttc_index(ttc_index);

      int added_font_index = font_unique_name_table.fonts_size() - 1;

      auto extract_and_append_names =
          [&font_unique_name_table, &hr, &font, &added_font_index](
              DWRITE_INFORMATIONAL_STRING_ID font_info_string_id) {
            // Now get names, and make them point to the added font.
            IDWriteLocalizedStrings* font_id_keyed_names;
            BOOL has_id_keyed_names;
            hr = font->GetInformationalStrings(
                font_info_string_id, &font_id_keyed_names, &has_id_keyed_names);
            if (FAILED(hr) || !has_id_keyed_names)
              return;

            std::vector<std::string> extracted_names;
            ExtractCaseFoldedLocalizedStrings(font_id_keyed_names,
                                              &extracted_names);
            for (auto& extracted_name : extracted_names) {
              blink::FontUniqueNameTable_UniqueNameToFontMapping* name_mapping =
                  font_unique_name_table.add_name_map();
              name_mapping->set_font_name(extracted_name);
              name_mapping->set_font_index(added_font_index);
            }
          };

      extract_and_append_names(DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME);
      extract_and_append_names(DWRITE_INFORMATIONAL_STRING_FULL_NAME);
    }

    if (UNLIKELY(slow_down_indexing_for_testing_))
      base::PlatformThread::Sleep(kIndexingSlowDownForTesting);
  }

  if (timed_out) {
    LOG(ERROR) << "Creating unique font lookup table timed out, emptying "
                  "partial table.";
    font_unique_name_table.clear_fonts();
    font_unique_name_table.clear_name_map();
  }

  // Sort names for using binary search on this proto in FontTableMatcher.
  std::sort(font_unique_name_table.mutable_name_map()->begin(),
            font_unique_name_table.mutable_name_map()->end(),
            [](const blink::FontUniqueNameTable_UniqueNameToFontMapping& a,
               const blink::FontUniqueNameTable_UniqueNameToFontMapping& b) {
              return a.font_name() < b.font_name();
            });

  font_table_memory_ = base::ReadOnlySharedMemoryRegion::Create(
      font_unique_name_table.ByteSizeLong());
  if (!IsFontUniqueNameTableValid()) {
    return;
  }

  if (!font_unique_name_table.SerializeToArray(
          font_table_memory_.mapping.memory(),
          font_table_memory_.mapping.size())) {
    font_table_memory_ = base::MappedReadOnlyRegion();
    return;
  }

  UMA_HISTOGRAM_TIMES("DirectWrite.Fonts.Proxy.LookupTableBuildTime",
                      base::TimeTicks::Now() - time_ticks);
  // The size is usually tens of kilobytes, ~50kb on a standard Windows 10
  // installation, 1MB should be a more than high enough upper limit.
  UMA_HISTOGRAM_CUSTOM_COUNTS("DirectWrite.Fonts.Proxy.LookupTableSize",
                              font_table_memory_.mapping.size() / 1024, 1, 1000,
                              50);
}

void DWriteFontLookupTableBuilder::SetSlowDownIndexingForTesting(
    bool slow_down) {
  slow_down_indexing_for_testing_ = slow_down;
}

void DWriteFontLookupTableBuilder::ResetLookupTableForTesting() {
  font_table_memory_ = base::MappedReadOnlyRegion();
}

DWriteFontLookupTableBuilder* DWriteFontLookupTableBuilder::GetInstance() {
  static base::NoDestructor<DWriteFontLookupTableBuilder> instance;
  return instance.get();
}

}  // namespace content
