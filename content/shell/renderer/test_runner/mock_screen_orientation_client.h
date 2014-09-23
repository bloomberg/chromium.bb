// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_SCREEN_ORIENTATION_CLIENT_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_SCREEN_ORIENTATION_CLIENT_H_

#include "base/macros.h"
#include "third_party/WebKit/public/platform/WebLockOrientationCallback.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationClient.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationLockType.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationType.h"

namespace blink {
class WebLocalFrame;
}

namespace content {

class MockScreenOrientationClient : public blink::WebScreenOrientationClient {
 public:
  explicit MockScreenOrientationClient();
  virtual ~MockScreenOrientationClient();

  void ResetData();
  void UpdateDeviceOrientation(blink::WebLocalFrame* main_frame,
                               blink::WebScreenOrientationType orientation);

  blink::WebScreenOrientationType CurrentOrientationType() const;
  unsigned CurrentOrientationAngle() const;

 private:
  // From blink::WebScreenOrientationClient.
  virtual void lockOrientation(blink::WebScreenOrientationLockType orientation,
                               blink::WebLockOrientationCallback* callback);
  virtual void unlockOrientation();

  void UpdateLockSync(blink::WebScreenOrientationLockType,
                      blink::WebLockOrientationCallback*);
  void ResetLockSync();

  void UpdateScreenOrientation(blink::WebScreenOrientationType);
  bool IsOrientationAllowedByCurrentLock(blink::WebScreenOrientationType);
  blink::WebScreenOrientationType SuitableOrientationForCurrentLock();
  static unsigned OrientationTypeToAngle(blink::WebScreenOrientationType);

  blink::WebLocalFrame* main_frame_;
  blink::WebScreenOrientationLockType current_lock_;
  blink::WebScreenOrientationType device_orientation_;
  blink::WebScreenOrientationType current_orientation_;

  DISALLOW_COPY_AND_ASSIGN(MockScreenOrientationClient);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_SCREEN_ORIENTATION_CONTROLLER_H_
