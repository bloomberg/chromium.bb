// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_ACCESSIBILITY_ACCESSIBILITY_SOUND_DELEGATE_H_
#define CHROMECAST_BROWSER_ACCESSIBILITY_ACCESSIBILITY_SOUND_DELEGATE_H_

namespace chromecast {
namespace shell {

// Provides audio feedback for TouchExplorationController.

class AccessibilitySoundDelegate {
 public:
  AccessibilitySoundDelegate() {}
  virtual ~AccessibilitySoundDelegate() {}
  virtual void PlayPassthroughEarcon() = 0;
  virtual void PlayEnterScreenEarcon() = 0;
  virtual void PlayExitScreenEarcon() = 0;
  virtual void PlayTouchTypeEarcon() = 0;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_ACCESSIBILITY_ACCESSIBILITY_SOUND_DELEGATE_H_
