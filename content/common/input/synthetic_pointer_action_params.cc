// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_pointer_action_params.h"

namespace content {

SyntheticPointerActionParams::SyntheticPointerActionParams()
    : pointer_action_type_(PointerActionType::NOT_INITIALIZED),
      index_(0),
      button_(Button::LEFT) {}

SyntheticPointerActionParams::SyntheticPointerActionParams(
    PointerActionType action_type)
    : pointer_action_type_(action_type), index_(0), button_(Button::LEFT) {}

SyntheticPointerActionParams::~SyntheticPointerActionParams() {}

// static
unsigned SyntheticPointerActionParams::GetWebMouseEventModifier(
    SyntheticPointerActionParams::Button button) {
  switch (button) {
    case SyntheticPointerActionParams::Button::LEFT:
      return blink::WebMouseEvent::LeftButtonDown;
    case SyntheticPointerActionParams::Button::MIDDLE:
      return blink::WebMouseEvent::MiddleButtonDown;
    case SyntheticPointerActionParams::Button::RIGHT:
      return blink::WebMouseEvent::RightButtonDown;
  }
  NOTREACHED();
  return blink::WebMouseEvent::NoModifiers;
}

// static
blink::WebMouseEvent::Button
SyntheticPointerActionParams::GetWebMouseEventButton(
    SyntheticPointerActionParams::Button button) {
  switch (button) {
    case SyntheticPointerActionParams::Button::LEFT:
      return blink::WebMouseEvent::Button::Left;
    case SyntheticPointerActionParams::Button::MIDDLE:
      return blink::WebMouseEvent::Button::Middle;
    case SyntheticPointerActionParams::Button::RIGHT:
      return blink::WebMouseEvent::Button::Right;
  }
  NOTREACHED();
  return blink::WebMouseEvent::Button::NoButton;
}

}  // namespace content