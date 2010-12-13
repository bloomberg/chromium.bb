// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/brightness_observer.h"

#include "chrome/browser/chromeos/brightness_bubble.h"
#include "chrome/browser/chromeos/volume_bubble.h"

namespace chromeos {

void BrightnessObserver::BrightnessChanged(int level) {
  BrightnessBubble::GetInstance()->ShowBubble(level);
  VolumeBubble::GetInstance()->HideBubble();
}

}  // namespace chromeos
