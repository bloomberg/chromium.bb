// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_ACCESSIBILITY_ACCESSIBILITY_SOUND_DELEGATE_STUB_H_
#define CHROMECAST_BROWSER_ACCESSIBILITY_ACCESSIBILITY_SOUND_DELEGATE_STUB_H_

#include "chromecast/browser/accessibility/accessibility_sound_delegate.h"

namespace chromecast {
namespace shell {

class AccessibilitySoundDelegateStub : public AccessibilitySoundDelegate {
 public:
  AccessibilitySoundDelegateStub() = default;
  ~AccessibilitySoundDelegateStub() override = default;
  void PlayPassthroughEarcon() override {}
  void PlayEnterScreenEarcon() override {}
  void PlayExitScreenEarcon() override {}
  void PlayTouchTypeEarcon() override {}
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_ACCESSIBILITY_ACCESSIBILITY_SOUND_DELEGATE_STUB_H_
