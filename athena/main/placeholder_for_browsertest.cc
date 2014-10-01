// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(skuhne): Please remove this file once SessionRestore is in place.

#include "athena/main/placeholder.h"

#include "athena/resources/grit/athena_resources.h"
#include "athena/system/public/system_ui.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

void CreateTestPages(content::BrowserContext* browser_context) {
}

void SetupBackgroundImage() {
  const gfx::ImageSkia wallpaper = *ui::ResourceBundle::GetSharedInstance()
      .GetImageSkiaNamed(IDR_ATHENA_BACKGROUND);
  athena::SystemUI::Get()->SetBackgroundImage(wallpaper);
}
