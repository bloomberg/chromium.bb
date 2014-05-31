// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/main/placeholder_content.h"

#include "athena/activity/public/activity_manager.h"
#include "athena/main/web_activity.h"
#include "content/public/browser/web_contents.h"

void CreateTestPages(content::BrowserContext* browser_context) {
  const char* kTestURLs[] = {
      "http://cyan.bikeshed.com", "https://www.google.com",
  };
  for (size_t i = 0; i < arraysize(kTestURLs); ++i) {
    content::WebContents::CreateParams params(browser_context);
    content::WebContents* contents = content::WebContents::Create(params);
    contents->GetController().LoadURL(GURL(kTestURLs[i]),
                                      content::Referrer(),
                                      content::PAGE_TRANSITION_TYPED,
                                      std::string());
    athena::ActivityManager::Get()->AddActivity(new WebActivity(contents));
    contents->Focus();
  }
}
