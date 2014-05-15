// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCREEN_ORIENTATION_MOCK_SCREEN_ORIENTATION_CONTROLLER_H_
#define CONTENT_RENDERER_SCREEN_ORIENTATION_MOCK_SCREEN_ORIENTATION_CONTROLLER_H_

#include "base/macros.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationLockType.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationType.h"

namespace blink {
class WebScreenOrientationListener;
}

namespace content {

class MockScreenOrientationController {
 public:
  MockScreenOrientationController();

  void SetListener(blink::WebScreenOrientationListener* listener);
  void ResetData();
  void UpdateLock(blink::WebScreenOrientationLockType);
  void ResetLock();
  void UpdateDeviceOrientation(blink::WebScreenOrientationType);

 private:
  void UpdateScreenOrientation(blink::WebScreenOrientationType);
  bool IsOrientationAllowedByCurrentLock(blink::WebScreenOrientationType);
  blink::WebScreenOrientationType SuitableOrientationForCurrentLock();

  blink::WebScreenOrientationLockType current_lock_;
  blink::WebScreenOrientationType device_orientation_;
  blink::WebScreenOrientationType current_orientation_;
  blink::WebScreenOrientationListener* listener_;

  DISALLOW_COPY_AND_ASSIGN(MockScreenOrientationController);
};

} // namespace content

#endif // CONTENT_RENDERER_SCREEN_ORIENTATION_MOCK_SCREEN_ORIENTATION_CONTROLLER_H_
