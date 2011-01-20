// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/volume_bubble.h"

#include "base/singleton.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

VolumeBubble::VolumeBubble()
    : SettingLevelBubble(
          ResourceBundle::GetSharedInstance().GetBitmapNamed(
              IDR_VOLUME_BUBBLE_UP_ICON),
          ResourceBundle::GetSharedInstance().GetBitmapNamed(
              IDR_VOLUME_BUBBLE_DOWN_ICON),
          ResourceBundle::GetSharedInstance().GetBitmapNamed(
              IDR_VOLUME_BUBBLE_MUTE_ICON)) {
}

// static
VolumeBubble* VolumeBubble::GetInstance() {
  return Singleton<VolumeBubble>::get();
}

}  // namespace chromeos
