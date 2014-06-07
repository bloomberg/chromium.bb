// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/main/placeholder.h"

#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "athena/screen/public/screen_manager.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/painter.h"

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
  gfx::Size size(200, 200);
  gfx::Canvas canvas(size, 1.0f, true);
  scoped_ptr<views::Painter> painter(
      views::Painter::CreateVerticalGradient(SK_ColorBLUE, SK_ColorCYAN));
  painter->Paint(&canvas, size);
  athena::ScreenManager::Get()->SetBackgroundImage(
      gfx::ImageSkia(canvas.ExtractImageRep()));
}
