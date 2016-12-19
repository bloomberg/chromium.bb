// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_PAGE_INFO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_PAGE_INFO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/weak_ptr.h"
#include "ios/chrome/browser/ui/omnibox/page_info_model_observer.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_controller.h"

@class BidiContainerView;
class PageInfoModel;

// TODO(crbug.com/227827) Merge 178763: PageInfoModel has been removed in
// upstream; check if we should use PageInfoModel.
// The view controller for the page info view.
@interface PageInfoViewController : NSObject
// Designated initializer.
// The |source| parameter should be in the coordinate system of the parent.
// Typically this would be the frame of a button that resulted in this popup
// being displayed.
- (id)initWithModel:(PageInfoModel*)model
             bridge:(PageInfoModelObserver*)bridge
        sourceFrame:(CGRect)source
         parentView:(UIView*)parent;

// Dismisses the view.
- (void)dismiss;

// Layout the page info view.
- (void)performLayout;

@end

// Bridge that listens for change notifications from the model.
class PageInfoModelBubbleBridge : public PageInfoModelObserver {
 public:
  PageInfoModelBubbleBridge();
  ~PageInfoModelBubbleBridge() override;

  // PageInfoModelObserver implementation.
  void OnPageInfoModelChanged() override;

  // Sets the controller.
  void set_controller(PageInfoViewController* controller) {
    controller_ = controller;
  }

 private:
  void PerformLayout() {
    // If the window is animating closed when this is called, the
    // animation could be holding the last reference to |controller_|
    // (and thus |this|).  Pin it until the task is completed.
    base::scoped_nsobject<PageInfoViewController> keep_alive(
        [controller_ retain]);
    [controller_ performLayout];
  }

  PageInfoViewController* controller_;  // weak

  base::WeakPtrFactory<PageInfoModelBubbleBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PageInfoModelBubbleBridge);
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_PAGE_INFO_VIEW_CONTROLLER_H_
