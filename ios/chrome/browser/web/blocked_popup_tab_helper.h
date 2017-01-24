// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_BLOCKED_POPUP_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_BLOCKED_POPUP_TAB_HELPER_H_

#import <UIKit/UIKit.h>
#include <vector>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "components/infobars/core/infobar_manager.h"
#import "ios/web/public/web_state/web_state_user_data.h"
#include "ios/web/web_state/blocked_popup_info.h"

// Handles blocked popups. Will display an infobar informing the user and
// allowing the user to add an exception and navigate to the site.
class BlockedPopupTabHelper
    : public infobars::InfoBarManager::Observer,
      public web::WebStateUserData<BlockedPopupTabHelper> {
 public:
  explicit BlockedPopupTabHelper(web::WebState* web_state);
  ~BlockedPopupTabHelper() override;

  // Shows the popup blocker infobar for the given popup.
  void HandlePopup(const web::BlockedPopupInfo& blocked_popup_info);

  // infobars::InfoBarManager::Observer implementation.
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
  void OnManagerShuttingDown(
      infobars::InfoBarManager* infobar_manager) override;

 private:
  friend class BlockedPopupTabHelperTest;

  // Shows the infobar for the current popups. Will also handle replacing an
  // existing infobar with the updated count.
  void ShowInfoBar();

  // Registers this object as an observer for the InfoBarManager associated with
  // |web_state_|.  Does nothing if already registered.
  void RegisterAsInfoBarManagerObserverIfNeeded(
      infobars::InfoBarManager* infobar_manager);

  // The WebState that this object is attached to.
  web::WebState* web_state_;
  // The currently displayed infobar.
  infobars::InfoBar* infobar_;
  // The popups to open.
  std::vector<web::BlockedPopupInfo> popups_;
  // For management of infobars::InfoBarManager::Observer registration.  This
  // object will not start observing the InfoBarManager until ShowInfoBars() is
  // called.
  ScopedObserver<infobars::InfoBarManager, infobars::InfoBarManager::Observer>
      scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(BlockedPopupTabHelper);
};

#endif  // IOS_CHROME_BROWSER_WEB_BLOCKED_POPUP_TAB_HELPER_H_
