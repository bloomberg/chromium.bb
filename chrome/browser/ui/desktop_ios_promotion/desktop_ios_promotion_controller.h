// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_DESKTOP_IOS_PROMOTION_DESKTOP_IOS_PROMOTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_DESKTOP_IOS_PROMOTION_DESKTOP_IOS_PROMOTION_CONTROLLER_H_

#include "base/macros.h"

namespace desktop_ios_promotion {
enum class PromotionEntryPoint;
}

// This class provides data to the Desktop to mobile promotion and control the
// promotion actions.
class DesktopIOSPromotionController {
 public:
  // Should be instantiated on the UI thread.
  DesktopIOSPromotionController();
  ~DesktopIOSPromotionController();

  // Called by the view code when "Send SMS" button is clicked by the user.
  void OnSendSMSClicked();

  // Called by the view code when "No Thanks" button is clicked by the user.
  void OnNoThanksClicked();

  // (TODO): If needed verify that functions that affect the UI are done
  // on the same thread.

  DISALLOW_COPY_AND_ASSIGN(DesktopIOSPromotionController);
};

#endif  // CHROME_BROWSER_UI_DESKTOP_IOS_PROMOTION_DESKTOP_IOS_PROMOTION_CONTROLLER_H_
