// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APP_LAUNCHED_ANIMATION_H_
#define CHROME_BROWSER_APP_LAUNCHED_ANIMATION_H_
#pragma once

#include "base/basictypes.h"

class Extension;

namespace gfx {
  class Rect;
}

class AppLaunchedAnimation {
 public:
  // Starts an animation of the |extension| being launched. The |rect| is the
  // rect of the app icon.
  static void Show(const Extension* extension, const gfx::Rect& rect);

 private:
  AppLaunchedAnimation() { }

  DISALLOW_COPY_AND_ASSIGN(AppLaunchedAnimation);
};

#endif  // CHROME_BROWSER_APP_LAUNCHED_ANIMATION_H_
