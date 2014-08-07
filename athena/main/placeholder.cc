// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/main/placeholder.h"

#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "athena/screen/public/screen_manager.h"
#include "grit/athena_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

void CreateTestPages(content::BrowserContext* browser_context) {
  const char* kTestURLs[] = {
      "http://cyan.bikeshed.com", "https://news.google.com",
      "http://blue.bikeshed.com", "https://www.google.com",
  };
  for (size_t i = 0; i < arraysize(kTestURLs); ++i) {
    athena::ActivityManager::Get()->AddActivity(
        athena::ActivityFactory::Get()->CreateWebActivity(browser_context,
                                                          GURL(kTestURLs[i])));
  }
}

void SetupBackgroundImage() {
  const gfx::ImageSkia wallpaper = *ui::ResourceBundle::GetSharedInstance()
      .GetImageSkiaNamed(IDR_ATHENA_BACKGROUND);
  athena::ScreenManager::Get()->SetBackgroundImage(wallpaper);
}
