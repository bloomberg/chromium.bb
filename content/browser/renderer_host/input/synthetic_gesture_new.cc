// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_gesture_new.h"

#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/synthetic_pinch_gesture_new.h"
#include "content/browser/renderer_host/input/synthetic_smooth_scroll_gesture_new.h"

namespace content {
namespace {

template <typename GestureType, typename GestureParamsType>
static scoped_ptr<SyntheticGestureNew> CreateGesture(
    const SyntheticGestureParams& gesture_params) {
  return scoped_ptr<SyntheticGestureNew>(
      new GestureType(*GestureParamsType::Cast(&gesture_params)));
}

}  // namespace

SyntheticGestureNew::SyntheticGestureNew() {}

SyntheticGestureNew::~SyntheticGestureNew() {}

scoped_ptr<SyntheticGestureNew> SyntheticGestureNew::Create(
    const SyntheticGestureParams& gesture_params) {
  switch (gesture_params.GetGestureType()) {
    case SyntheticGestureParams::SMOOTH_SCROLL_GESTURE:
      return CreateGesture<SyntheticSmoothScrollGestureNew,
                           SyntheticSmoothScrollGestureParams>(gesture_params);
    case SyntheticGestureParams::PINCH_GESTURE:
      return CreateGesture<SyntheticPinchGestureNew,
                           SyntheticPinchGestureParams>(gesture_params);
  }
  NOTREACHED() << "Invalid synthetic gesture type";
  return scoped_ptr<SyntheticGestureNew>();
}

}  // namespace content
