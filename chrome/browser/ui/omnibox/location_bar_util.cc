// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/location_bar_util.h"

#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "ui/base/l10n/l10n_util.h"

namespace location_bar_util {

std::wstring GetKeywordName(Profile* profile, const std::wstring& keyword) {
// Make sure the TemplateURL still exists.
// TODO(sky): Once LocationBarView adds a listener to the TemplateURLModel
// to track changes to the model, this should become a DCHECK.
  const TemplateURL* template_url =
      profile->GetTemplateURLModel()->GetTemplateURLForKeyword(
          WideToUTF16Hack(keyword));
  if (template_url)
    return UTF16ToWideHack(template_url->AdjustedShortNameForLocaleDirection());
  return std::wstring();
}

std::wstring CalculateMinString(const std::wstring& description) {
  // Chop at the first '.' or whitespace.
  const size_t dot_index = description.find(L'.');
  const size_t ws_index = description.find_first_of(kWhitespaceWide);
  size_t chop_index = std::min(dot_index, ws_index);
  std::wstring min_string;
  if (chop_index == std::wstring::npos) {
    // No dot or whitespace, truncate to at most 3 chars.
    min_string = UTF16ToWideHack(
        l10n_util::TruncateString(WideToUTF16Hack(description), 3));
  } else {
    min_string = description.substr(0, chop_index);
  }
  base::i18n::AdjustStringForLocaleDirection(&min_string);
  return min_string;
}

}  // namespace location_bar_util
