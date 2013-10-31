// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_TARGET_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_TARGET_H_

#include "base/time/time.h"
#include "content/browser/renderer_host/input/synthetic_gesture_new.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_gesture_params.h"

namespace content {

class InputEvent;

// Interface between the synthetic gesture controller and the RWHV.
class CONTENT_EXPORT SyntheticGestureTarget {
 public:
  SyntheticGestureTarget() {}
  virtual ~SyntheticGestureTarget() {}

  virtual void QueueInputEventToPlatform(const InputEvent& event) = 0;

  virtual void OnSyntheticGestureCompleted(
      SyntheticGestureNew::Result result) = 0;

  virtual base::TimeDelta GetSyntheticGestureUpdateRate() const = 0;

  virtual SyntheticGestureParams::GestureSourceType
      GetDefaultSyntheticGestureSourceType() const = 0;
  virtual bool SupportsSyntheticGestureSourceType(
      SyntheticGestureParams::GestureSourceType gesture_source_type) const = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_TARGET_H_
