// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_SCROLL_END_EFFECT_CONTROLLER_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_SCROLL_END_EFFECT_CONTROLLER_ASH_H_

#include "base/compiler_specific.h"
#include "chrome/browser/ui/views/frame/scroll_end_effect_controller.h"

class ScrollEndEffectControllerAsh : public ScrollEndEffectController {
 public:
  ScrollEndEffectControllerAsh();
  virtual ~ScrollEndEffectControllerAsh();

  // ScrollEndEffectController overides:
  virtual void OverscrollUpdate(int delta_y) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScrollEndEffectControllerAsh);
 };

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_SCROLL_END_EFFECT_CONTROLLER_ASH_H_
