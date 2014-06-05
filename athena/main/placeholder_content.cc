// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/main/placeholder_content.h"

#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"

void CreateTestPages(content::BrowserContext* browser_context) {
  const char* kTestURLs[] = {
      "http://cyan.bikeshed.com", "https://news.google.com",
      "http://blue.bikeshed.com", "https://www.google.com",
  };
  for (size_t i = 0; i < arraysize(kTestURLs); ++i) {
    athena::ActivityManager::Get()->AddActivity(
        athena::ActivityFactory::Get()->CreateWebActivity(
            browser_context, GURL(kTestURLs[i])));
  }
}
