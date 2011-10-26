// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/top_sites_extension_api.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"

GetTopSitesFunction::GetTopSitesFunction() {
}

GetTopSitesFunction::~GetTopSitesFunction() {
}

bool GetTopSitesFunction::RunImpl() {
  history::TopSites* ts = profile()->GetTopSites();
  if (!ts)
    return false;

  ts->GetMostVisitedURLs(
      &topsites_consumer_,
      base::Bind(&GetTopSitesFunction::OnMostVisitedURLsAvailable, this));
  return true;
}

void GetTopSitesFunction::OnMostVisitedURLsAvailable(
    const history::MostVisitedURLList& data) {
  // Code is a direct rip from most_visited_handler.cc TODO(estade): unfork.
  scoped_ptr<base::ListValue> pages_value(new ListValue);
  for (size_t i = 0; i < data.size(); i++) {
    const history::MostVisitedURL& url = data[i];
    DictionaryValue* page_value = new DictionaryValue();
    if (url.url.is_empty()) {
      page_value->SetBoolean("filler", true);
      pages_value->Append(page_value);
      continue;
    }

    NewTabUI::SetURLTitleAndDirection(page_value,
                                      url.title,
                                      url.url);

    pages_value->Append(page_value);
  }
  // End copied code. ----------------------------------------------------------

  result_.reset(pages_value.release());
  SendResponse(true);
}
