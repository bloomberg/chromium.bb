// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SCREEN_CAPTURE_SCREEN_CAPTURE_OBSERVER_H_
#define CHROMEOS_SCREEN_CAPTURE_SCREEN_CAPTURE_OBSERVER_H_

namespace chromeos {

class ScreenCaptureObserver {
 public:
  virtual ~ScreenCaptureObserver() {}

  virtual void OnScreenCaptureStatusChanged() = 0;
};

};  // namespace chromeos

#endif  // CHROMEOS_SCREEN_CAPTURE_SCREEN_CAPTURE_OBSERVER_H_
