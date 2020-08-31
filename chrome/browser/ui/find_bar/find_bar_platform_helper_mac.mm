// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#import "chrome/browser/ui/find_bar/find_bar_platform_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/find_in_page/find_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#import "ui/base/cocoa/find_pasteboard.h"

namespace {

class FindBarPlatformHelperMac : public FindBarPlatformHelper {
 public:
  FindBarPlatformHelperMac(FindBarController* find_bar_controller)
      : FindBarPlatformHelper(find_bar_controller) {
    find_pasteboard_notification_observer_ =
        [[NSNotificationCenter defaultCenter]
            addObserverForName:kFindPasteboardChangedNotification
                        object:[FindPasteboard sharedInstance]
                         queue:nil
                    usingBlock:^(NSNotification*) {
                      UpdateFindBarControllerFromPasteboard();
                    }];
    UpdateFindBarControllerFromPasteboard();
  }

  ~FindBarPlatformHelperMac() override {
    [[NSNotificationCenter defaultCenter]
        removeObserver:find_pasteboard_notification_observer_];
  }

  void OnUserChangedFindText(base::string16 text) override {
    if (find_bar_controller_->web_contents()
            ->GetBrowserContext()
            ->IsOffTheRecord()) {
      return;
    }

    [[FindPasteboard sharedInstance]
        setFindText:base::SysUTF16ToNSString(text)];
  }

 private:
  void UpdateFindBarControllerFromPasteboard() {
    content::WebContents* active_web_contents =
        find_bar_controller_->web_contents();
    Browser* browser =
        active_web_contents
            ? chrome::FindBrowserWithWebContents(active_web_contents)
            : nullptr;
    if (browser) {
      TabStripModel* tab_strip_model = browser->tab_strip_model();

      for (int i = 0; i < tab_strip_model->count(); ++i) {
        content::WebContents* web_contents =
            tab_strip_model->GetWebContentsAt(i);
        if (active_web_contents == web_contents)
          continue;
        find_in_page::FindTabHelper* find_tab_helper =
            find_in_page::FindTabHelper::FromWebContents(web_contents);
        find_tab_helper->StopFinding(find_in_page::SelectionAction::kClear);
      }
    }

    NSString* find_text = [[FindPasteboard sharedInstance] findText];
    find_bar_controller_->SetText(base::SysNSStringToUTF16(find_text));
  }

  id find_pasteboard_notification_observer_;

  DISALLOW_COPY_AND_ASSIGN(FindBarPlatformHelperMac);
};

}  // namespace

// static
std::unique_ptr<FindBarPlatformHelper> FindBarPlatformHelper::Create(
    FindBarController* find_bar_controller) {
  return std::make_unique<FindBarPlatformHelperMac>(find_bar_controller);
}
