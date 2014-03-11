// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#include "components/translate/core/common/translate_errors.h"

@class BrowserWindowController;

class TranslateBubbleModel;

namespace content {
class WebContents;
}

// Displays the Translate bubble. The Translate bubble is a bubble which
// pops up when clicking the Translate icon on Omnibox. This bubble
// allows us to translate a foreign page into user-selected language,
// revert this, and configure the translate setting.
@interface TranslateBubbleController : BaseBubbleController {
 @private
  content::WebContents* webContents_;
  scoped_ptr<TranslateBubbleModel> model_;

  // Whether the translation is actually executed once at least.
  BOOL translateExecuted_;
}

@property(readonly, nonatomic) const content::WebContents* webContents;
@property(readonly, nonatomic) const TranslateBubbleModel* model;

- (id)initWithParentWindow:(BrowserWindowController*)controller
                     model:(scoped_ptr<TranslateBubbleModel>)model
               webContents:(content::WebContents*)webContents;
- (void)switchView:(TranslateBubbleModel::ViewState)viewState;
- (void)switchToErrorView:(TranslateErrors::Type)errorType;

@end
