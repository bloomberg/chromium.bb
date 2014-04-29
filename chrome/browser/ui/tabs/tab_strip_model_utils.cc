// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model_utils.h"

#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"

namespace chrome {

void GetOpenUrls(const TabStripModel& tabs,
                 const history::TopSites& top_sites,
                 std::set<std::string>* urls) {
  for (int i = 0; i < tabs.count(); ++i) {
    content::WebContents* web_contents = tabs.GetWebContentsAt(i);
    if (web_contents)
      urls->insert(top_sites.GetCanonicalURLString(web_contents->GetURL()));
  }
}

}  // namespace chrome
