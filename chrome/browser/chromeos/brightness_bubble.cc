// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/brightness_bubble.h"

#include "base/singleton.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

BrightnessBubble::BrightnessBubble()
    : SettingLevelBubble(
          ResourceBundle::GetSharedInstance().GetBitmapNamed(
              IDR_BRIGHTNESS_BUBBLE_ICON),
          ResourceBundle::GetSharedInstance().GetBitmapNamed(
              IDR_BRIGHTNESS_BUBBLE_ICON),
          ResourceBundle::GetSharedInstance().GetBitmapNamed(
              IDR_BRIGHTNESS_BUBBLE_ICON)) {
}

// static
BrightnessBubble* BrightnessBubble::GetInstance() {
  return Singleton<BrightnessBubble>::get();
}

}  // namespace chromeos
