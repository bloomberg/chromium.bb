// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NEW_FOREGROUND_TAB_FULLSCREEN_DISABLER_H_
#define IOS_CHROME_BROWSER_UI_NEW_FOREGROUND_TAB_FULLSCREEN_DISABLER_H_

#include <memory>

#import "ios/chrome/browser/ui/fullscreen/animated_scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"

class FullscreenController;
class WebStateList;

// Helper object that shows the toolbar when a new foreground tab is inserted.
class NewForegroundTabFullscreenDisabler
    : public AnimatedScopedFullscreenDisablerObserver,
      public WebStateListObserver {
 public:
  NewForegroundTabFullscreenDisabler(WebStateList* web_state_list,
                                     FullscreenController* controller);
  ~NewForegroundTabFullscreenDisabler() override;

  // Stops observing the WebStateList and disabling the FullscreenController.
  void Disconnect();

 private:
  // AnimatedScopedFullscreenDisablerObserver:
  void FullscreenDisablingAnimationDidFinish(
      AnimatedScopedFullscreenDisabler* disabler) override;

  // WebStateListObserver:
  void WebStateInsertedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index,
                          bool activating) override;

  // The WebStateList.
  WebStateList* web_state_list_;
  // The FullscreenController.
  FullscreenController* controller_;
  // The disabler used for new foreground tabs.
  std::unique_ptr<AnimatedScopedFullscreenDisabler> disabler_;
};

#endif  // IOS_CHROME_BROWSER_UI_NEW_FOREGROUND_TAB_FULLSCREEN_DISABLER_H_
