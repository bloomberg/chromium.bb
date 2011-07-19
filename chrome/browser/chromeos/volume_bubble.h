// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_VOLUME_BUBBLE_H_
#define CHROME_BROWSER_CHROMEOS_VOLUME_BUBBLE_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/chromeos/setting_level_bubble.h"

template <typename T> struct DefaultSingletonTraits;

namespace chromeos {

// Singleton class controlling volume bubble.
class VolumeBubble : public SettingLevelBubble {
 public:
  static VolumeBubble* GetInstance();

 private:
  friend struct DefaultSingletonTraits<VolumeBubble>;

  VolumeBubble();
  virtual ~VolumeBubble() {}

  DISALLOW_COPY_AND_ASSIGN(VolumeBubble);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_VOLUME_BUBBLE_H_
