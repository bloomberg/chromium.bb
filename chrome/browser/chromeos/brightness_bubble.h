// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BRIGHTNESS_BUBBLE_H_
#define CHROME_BROWSER_CHROMEOS_BRIGHTNESS_BUBBLE_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/chromeos/setting_level_bubble.h"

template <typename T> struct DefaultSingletonTraits;

namespace chromeos {

// Singleton class controlling brightness bubble.
class BrightnessBubble : public SettingLevelBubble {
 public:
  static BrightnessBubble* GetInstance();

 private:
  friend struct DefaultSingletonTraits<BrightnessBubble>;

  BrightnessBubble();
  virtual ~BrightnessBubble() {}

  DISALLOW_COPY_AND_ASSIGN(BrightnessBubble);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_BRIGHTNESS_BUBBLE_H_
