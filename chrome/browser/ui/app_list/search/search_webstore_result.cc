// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_webstore_result.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/extensions/extension_constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace app_list {

SearchWebstoreResult::SearchWebstoreResult(Profile* profile,
                                           const std::string& query)
    : profile_(profile),
      query_(query),
      launch_url_(extension_urls::GetWebstoreSearchPageUrl(query)) {
  set_id(launch_url_.spec());
  set_relevance(0.0);

  set_title(base::UTF8ToUTF16(query));

  const base::string16 details =
      l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE);
  Tags details_tags;
  details_tags.push_back(Tag(SearchResult::Tag::DIM, 0, details.length()));

  set_details(details);
  set_details_tags(details_tags);

  SetIcon(*ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_WEBSTORE_ICON_32));
}

SearchWebstoreResult::~SearchWebstoreResult() {}

void SearchWebstoreResult::Open(int event_flags) {
  const GURL store_url = net::AppendQueryParameter(
      launch_url_,
      extension_urls::kWebstoreSourceField,
      extension_urls::kLaunchSourceAppListSearch);

  chrome::NavigateParams params(profile_,
                                store_url,
                                content::PAGE_TRANSITION_LINK);
  params.disposition = ui::DispositionFromEventFlags(event_flags);
  chrome::Navigate(&params);
}

void SearchWebstoreResult::InvokeAction(int action_index, int event_flags) {
}

scoped_ptr<ChromeSearchResult> SearchWebstoreResult::Duplicate() {
  return scoped_ptr<ChromeSearchResult>(
      new SearchWebstoreResult(profile_, query_)).Pass();
}

ChromeSearchResultType SearchWebstoreResult::GetType() {
  return WEBSTORE_SEARCH_RESULT;
}

}  // namespace app_list
