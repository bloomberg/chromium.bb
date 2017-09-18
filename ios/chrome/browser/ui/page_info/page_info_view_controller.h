// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/weak_ptr.h"
#include "ios/chrome/browser/ui/page_info/page_info_model_observer.h"

@class BidiContainerView;
@protocol BrowserCommands;
class PageInfoModel;

// TODO(crbug.com/227827) Merge 178763: PageInfoModel has been removed in
// upstream; check if we should use PageInfoModel.
// The view controller for the page info view.
@interface PageInfoViewController : NSObject
// Designated initializer.
// The |source| parameter should be in the coordinate system of the parent.
// Typically this would be the midpoint of a button that resulted in this popup
// being displayed.
- (id)initWithModel:(PageInfoModel*)model
             bridge:(PageInfoModelObserver*)bridge
        sourcePoint:(CGPoint)sourcePoint
         parentView:(UIView*)parent
         dispatcher:(id<BrowserCommands>)dispatcher;

// Dispatcher for this view controller.
@property(nonatomic, weak) id<BrowserCommands> dispatcher;

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
  void PerformLayout();

  __weak PageInfoViewController* controller_ = nil;

  base::WeakPtrFactory<PageInfoModelBubbleBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PageInfoModelBubbleBridge);
};

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_VIEW_CONTROLLER_H_
