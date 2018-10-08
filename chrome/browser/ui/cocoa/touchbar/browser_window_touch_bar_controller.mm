// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/touchbar/browser_window_touch_bar_controller.h"

#include <memory>

#include "base/mac/availability.h"
#include "base/mac/mac_util.h"
#import "base/mac/scoped_nsobject.h"
#import "base/mac/sdk_forward_declarations.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/touchbar/browser_window_default_touch_bar.h"
#import "chrome/browser/ui/cocoa/touchbar/web_textfield_touch_bar_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#import "ui/base/cocoa/touch_bar_util.h"

class API_AVAILABLE(macos(10.12.2)) WebContentsNotificationBridge
    : public TabStripModelObserver,
      public content::WebContentsObserver {
 public:
  WebContentsNotificationBridge(BrowserWindowTouchBarController* owner,
                                Browser* browser)
      : owner_(owner), browser_(browser), contents_(nullptr) {
    TabStripModel* model = browser_->tab_strip_model();
    DCHECK(model);
    model->AddObserver(this);

    UpdateWebContents(model->GetActiveWebContents());
  }

  ~WebContentsNotificationBridge() override {
    TabStripModel* model = browser_->tab_strip_model();
    if (model)
      model->RemoveObserver(this);
  }

  void UpdateWebContents(content::WebContents* new_contents) {
    contents_ = new_contents;
    Observe(contents_);

    [owner_ updateWebContents:contents_];
  }

  // TabStripModelObserver:
  void ActiveTabChanged(content::WebContents* old_contents,
                        content::WebContents* new_contents,
                        int index,
                        int reason) override {
    UpdateWebContents(new_contents);
    contents_ = new_contents;
  }

  content::WebContents* contents() const { return contents_; }

 protected:
  // WebContentsObserver:
  void WebContentsDestroyed() override {
    // Clean up if the web contents is being destroyed.
    UpdateWebContents(nullptr);
  }

 private:
  BrowserWindowTouchBarController* owner_;  // Weak.
  Browser* browser_;                        // Weak.
  content::WebContents* contents_;          // Weak.
};

@interface BrowserWindowTouchBarController () {
  NSWindow* window_;  // Weak.

  // Used to receive and handle notifications.
  std::unique_ptr<WebContentsNotificationBridge> notificationBridge_;

  base::scoped_nsobject<BrowserWindowDefaultTouchBar> defaultTouchBar_;

  base::scoped_nsobject<WebTextfieldTouchBarController> webTextfieldTouchBar_;
}
@end

@implementation BrowserWindowTouchBarController

- (instancetype)initWithBrowser:(Browser*)browser window:(NSWindow*)window {
  if ((self = [super init])) {
    DCHECK(browser);
    window_ = window;

    notificationBridge_ =
        std::make_unique<WebContentsNotificationBridge>(self, browser);

    defaultTouchBar_.reset([[BrowserWindowDefaultTouchBar alloc]
        initWithBrowser:browser
             controller:self]);
    webTextfieldTouchBar_.reset(
        [[WebTextfieldTouchBarController alloc] initWithController:self]);
  }

  return self;
}

- (void)invalidateTouchBar {
  DCHECK([window_ respondsToSelector:@selector(setTouchBar:)]);
  [window_ performSelector:@selector(setTouchBar:) withObject:nil];
}

- (NSTouchBar*)makeTouchBar {
  NSTouchBar* touchBar = [webTextfieldTouchBar_ makeTouchBar];
  if (touchBar)
    return touchBar;

  return [defaultTouchBar_ makeTouchBar];
}

- (void)updateWebContents:(content::WebContents*)contents {
  [defaultTouchBar_ updateWebContents:contents];
  [webTextfieldTouchBar_ updateWebContents:contents];
  [self invalidateTouchBar];
}

- (content::WebContents*)webContents {
  return notificationBridge_->web_contents();
}

@end

@implementation BrowserWindowTouchBarController (ExposedForTesting)

- (BrowserWindowDefaultTouchBar*)defaultTouchBar {
  return defaultTouchBar_.get();
}

- (WebTextfieldTouchBarController*)webTextfieldTouchBar {
  return webTextfieldTouchBar_.get();
}

@end
