// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/top_sites/top_sites_api.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"

namespace extensions {

TopSitesGetFunction::TopSitesGetFunction()
    : weak_ptr_factory_(this) {}

TopSitesGetFunction::~TopSitesGetFunction() {}

bool TopSitesGetFunction::RunAsync() {
  history::TopSites* ts = GetProfile()->GetTopSites();
  if (!ts)
    return false;

  ts->GetMostVisitedURLs(
      base::Bind(&TopSitesGetFunction::OnMostVisitedURLsAvailable,
                 weak_ptr_factory_.GetWeakPtr()), false);
  return true;
}

void TopSitesGetFunction::OnMostVisitedURLsAvailable(
    const history::MostVisitedURLList& data) {
  scoped_ptr<base::ListValue> pages_value(new base::ListValue);
  for (size_t i = 0; i < data.size(); i++) {
    const history::MostVisitedURL& url = data[i];
    if (!url.url.is_empty()) {
      base::DictionaryValue* page_value = new base::DictionaryValue();
      page_value->SetString("url", url.url.spec());
      if (url.title.empty())
        page_value->SetString("title", url.url.spec());
      else
        page_value->SetString("title", url.title);
      pages_value->Append(page_value);
    }
  }

  SetResult(pages_value.release());
  SendResponse(true);
}

}  // namespace extensions
