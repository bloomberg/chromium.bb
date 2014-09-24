// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/main/placeholder.h"

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_factory.h"
#include "athena/resources/grit/athena_resources.h"
#include "athena/system/public/system_ui.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

void CreateTestPages(content::BrowserContext* browser_context) {
  const char* kTestURLs[] = {
      "http://cyan.bikeshed.com", "https://news.google.com",
      "http://blue.bikeshed.com", "https://www.google.com",
  };
  athena::ActivityFactory* factory = athena::ActivityFactory::Get();
  for (size_t i = 0; i < arraysize(kTestURLs); ++i) {
    athena::Activity* activity = factory->CreateWebActivity(
        browser_context, base::string16(), GURL(kTestURLs[i]));
    athena::Activity::Show(activity);
  }
}

void SetupBackgroundImage() {
  const gfx::ImageSkia wallpaper = *ui::ResourceBundle::GetSharedInstance()
      .GetImageSkiaNamed(IDR_ATHENA_BACKGROUND);
  athena::SystemUI::Get()->SetBackgroundImage(wallpaper);
}
