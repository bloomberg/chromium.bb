// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/translate/translate_bubble_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/translate/translate_ui_delegate.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/translate/translate_bubble_model_impl.h"
#import "ui/base/cocoa/flipped_view.h"
#import "ui/base/cocoa/window_size_constants.h"

@implementation TranslateBubbleController

- (id)initWithParentWindow:(BrowserWindowController*)controller
                     model:(scoped_ptr<TranslateBubbleModel>)model
               webContents:(content::WebContents*)webContents {
  NSWindow* parentWindow = [controller window];

  // Use an arbitrary size; it will be changed in performLayout.
  NSRect contentRect = ui::kWindowSizeDeterminedLater;
  base::scoped_nsobject<InfoBubbleWindow> window(
      [[InfoBubbleWindow alloc] initWithContentRect:contentRect
                                          styleMask:NSBorderlessWindowMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO]);

  if ((self = [super initWithWindow:window
                       parentWindow:parentWindow
                         anchoredAt:NSZeroPoint])) {
    webContents_ = webContents;
    model_ = model.Pass();
    if (model_->GetViewState() !=
        TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE) {
      translateExecuted_ = YES;
    }

    [self performLayout];
  }
  return self;
}

@synthesize webContents = webContents_;

- (const TranslateBubbleModel*)model {
  return model_.get();
}

- (void)showWindow:(id)sender {
  BrowserWindowController* controller = [[self parentWindow] windowController];
  NSPoint anchorPoint = [[controller toolbarController] translateBubblePoint];
  anchorPoint = [[self parentWindow] convertBaseToScreen:anchorPoint];
  [self setAnchorPoint:anchorPoint];
  [super showWindow:sender];
}

- (void)switchView:(TranslateBubbleModel::ViewState)viewState {
  if (model_->GetViewState() == viewState)
    return;

  model_->SetViewState(viewState);
  [self performLayout];
}

- (void)switchToErrorView:(TranslateErrors::Type)errorType {
  // TODO(hajimehoshi): Implement this.
}

- (void)performLayout {
  // TODO(hajimehoshi): Now this shows just an empty bubble. Implement this.
  [[self window] display];
}

@end
