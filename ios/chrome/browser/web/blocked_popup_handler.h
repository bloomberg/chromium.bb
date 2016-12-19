// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_BLOCKED_POPUP_HANDLER_H_
#define IOS_CHROME_BROWSER_WEB_BLOCKED_POPUP_HANDLER_H_

#import <UIKit/UIKit.h>
#include <vector>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "components/infobars/core/infobar_manager.h"
#include "ios/web/web_state/blocked_popup_info.h"
#include "url/gurl.h"

namespace ios {
class ChromeBrowserState;
}

@protocol BlockedPopupHandlerDelegate;

// Handles blocked popups. Will display an infobar informing the user and
// allowing the user to add an exception and navigate to the site.
class BlockedPopupHandler : public infobars::InfoBarManager::Observer {
 public:
  explicit BlockedPopupHandler(ios::ChromeBrowserState* browser_state);
  ~BlockedPopupHandler() override;

  void SetDelegate(id<BlockedPopupHandlerDelegate> delegate);

  // Show the popup blocker infobar for the given popup.
  void HandlePopup(const web::BlockedPopupInfo& blocked_popup_info);

  // infobars::InfoBarManager::Observer implementation.
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
  void OnManagerShuttingDown(
      infobars::InfoBarManager* infobar_manager) override;

 private:
  // Show the infobar for the current popups. Will also handle replacing an
  // existing infobar with the updated count.
  void ShowInfoBar();

  // The user browser state.
  ios::ChromeBrowserState* browser_state_;
  // The delegate that will display the infobar.
  id<BlockedPopupHandlerDelegate> delegate_;
  // The currently displayed infobar.
  infobars::InfoBar* infobar_;
  // The popups to open.
  std::vector<web::BlockedPopupInfo> popups_;
  // For management of infobars::InfoBarManager::Observer registration.
  ScopedObserver<infobars::InfoBarManager, infobars::InfoBarManager::Observer>
      scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(BlockedPopupHandler);
};

// Methods implemented by the delegate of the BlockedPopupHandler.
@protocol BlockedPopupHandlerDelegate

// Returns the infobars::InfoBarManager.
- (infobars::InfoBarManager*)infoBarManager;

@end

#endif  // IOS_CHROME_BROWSER_WEB_BLOCKED_POPUP_HANDLER_H_
