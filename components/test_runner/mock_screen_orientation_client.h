// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_MOCK_SCREEN_ORIENTATION_CLIENT_H_
#define COMPONENTS_TEST_RUNNER_MOCK_SCREEN_ORIENTATION_CLIENT_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/test_runner/test_runner_export.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebLockOrientationCallback.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationClient.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationType.h"

namespace blink {
class WebLocalFrame;
}

namespace test_runner {

class TEST_RUNNER_EXPORT MockScreenOrientationClient
    : public NON_EXPORTED_BASE(blink::WebScreenOrientationClient) {
 public:
  explicit MockScreenOrientationClient();
  ~MockScreenOrientationClient() override;

  void ResetData();
  void UpdateDeviceOrientation(blink::WebLocalFrame* main_frame,
                               blink::WebScreenOrientationType orientation);

  blink::WebScreenOrientationType CurrentOrientationType() const;
  unsigned CurrentOrientationAngle() const;
  bool IsDisabled() const { return is_disabled_; }
  void SetDisabled(bool disabled);

 private:
  // From blink::WebScreenOrientationClient.
  void lockOrientation(
      blink::WebScreenOrientationLockType orientation,
      std::unique_ptr<blink::WebLockOrientationCallback> callback) override;
  void unlockOrientation() override;

  void UpdateLockSync(blink::WebScreenOrientationLockType,
                      std::unique_ptr<blink::WebLockOrientationCallback>);
  void ResetLockSync();

  void UpdateScreenOrientation(blink::WebScreenOrientationType);
  bool IsOrientationAllowedByCurrentLock(blink::WebScreenOrientationType);
  blink::WebScreenOrientationType SuitableOrientationForCurrentLock();
  static unsigned OrientationTypeToAngle(blink::WebScreenOrientationType);

  blink::WebLocalFrame* main_frame_;
  blink::WebScreenOrientationLockType current_lock_;
  blink::WebScreenOrientationType device_orientation_;
  blink::WebScreenOrientationType current_orientation_;
  bool is_disabled_;

  DISALLOW_COPY_AND_ASSIGN(MockScreenOrientationClient);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_MOCK_SCREEN_ORIENTATION_CLIENT_H_
