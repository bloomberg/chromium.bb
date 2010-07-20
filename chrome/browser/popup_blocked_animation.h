// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POPUP_BLOCKED_ANIMATION_H_
#define CHROME_BROWSER_POPUP_BLOCKED_ANIMATION_H_

#include "base/basictypes.h"

class TabContents;

// The popup blocker lives in the Omnibox with the rest of the blocked content
// controls. This icon is hard to notice when it appears as a result of user-
// initiated action. This creates an animation from the center of the window
// to the blocked popup icon in the Omnibox to draw the user's attention to it.
class PopupBlockedAnimation {
 public:
  static void Show(TabContents* tab_contents);

 private:
  PopupBlockedAnimation() {}
  DISALLOW_COPY_AND_ASSIGN(PopupBlockedAnimation);
};

#endif  // CHROME_BROWSER_POPUP_BLOCKED_ANIMATION_H_
