// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/location_bar_util.h"

#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/text_elider.h"

namespace location_bar_util {

std::wstring GetKeywordName(Profile* profile, const std::wstring& keyword) {
// Make sure the TemplateURL still exists.
// TODO(sky): Once LocationBarView adds a listener to the TemplateURLService
// to track changes to the model, this should become a DCHECK.
  const TemplateURL* template_url =
      TemplateURLServiceFactory::GetForProfile(profile)->
      GetTemplateURLForKeyword(WideToUTF16Hack(keyword));
  if (template_url)
    return UTF16ToWideHack(template_url->AdjustedShortNameForLocaleDirection());
  return std::wstring();
}

string16 CalculateMinString(const string16& description) {
  // Chop at the first '.' or whitespace.
  const size_t dot_index = description.find('.');
  const size_t ws_index = description.find_first_of(kWhitespaceUTF16);
  size_t chop_index = std::min(dot_index, ws_index);
  string16 min_string;
  if (chop_index == string16::npos) {
    // No dot or whitespace, truncate to at most 3 chars.
    min_string = ui::TruncateString(description, 3);
  } else {
    min_string = description.substr(0, chop_index);
  }
  base::i18n::AdjustStringForLocaleDirection(&min_string);
  return min_string;
}

}  // namespace location_bar_util
