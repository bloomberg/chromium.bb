// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOUCH_ANIMATION_SCREEN_ROTATION_SETTER_H_
#define CHROME_BROWSER_UI_TOUCH_ANIMATION_SCREEN_ROTATION_SETTER_H_
#pragma once

#include "views/layer_property_setter.h"

namespace views {
class View;
}

class ScreenRotationSetterFactory {
 public:
  // The setter will not own the view, and it is required that the view
  // outlive the setter.
  static views::LayerPropertySetter* Create(views::View* view);
};

#endif  // CHROME_BROWSER_UI_TOUCH_ANIMATION_SCREEN_ROTATION_SETTER_H_
