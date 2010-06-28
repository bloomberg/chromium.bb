// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/extensions/extension_infobar_controller.h"

#import "chrome/browser/cocoa/animatable_view.h"
#include "chrome/browser/cocoa/infobar.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"

namespace {
const CGFloat kAnimationDuration = 0.12;
const CGFloat kBottomBorderHeightPx = 1.0;
}  // namepsace

@interface ExtensionInfoBarController(Private)
// Called when the extension's hosted NSView has been resized.
- (void)extensionViewFrameChanged;
// Adjusts the width of the extension's hosted view to match the window's width.
- (void)adjustWidthToFitWindow;
@end

@implementation ExtensionInfoBarController

- (id)initWithDelegate:(InfoBarDelegate*)delegate
                window:(NSWindow*)window {
  if ((self = [super initWithDelegate:delegate])) {
    window_ = window;
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)addAdditionalControls {
  [self removeButtons];

  extensionView_ = delegate_->AsExtensionInfoBarDelegate()->
      extension_host()->view()->native_view();

  // Add the extension's RenderWidgetHostViewMac to the view hierarchy of the
  // InfoBar and make sure to place it below the Close button.
  [infoBarView_ addSubview:extensionView_
                positioned:NSWindowBelow
                relativeTo:(NSView*)closeButton_];

  // Because the parent view has a bottom border, account for it during
  // positioning.
  NSRect extensionFrame = [extensionView_ frame];
  extensionFrame.origin.y = kBottomBorderHeightPx;

  [extensionView_ setFrame:extensionFrame];
  // The extension's native view will only have a height that is non-zero if it
  // already has been loaded and rendered, which is the case when you switch
  // back to a tab with an extension infobar within it. The reason this is
  // needed is because the extension view's frame will not have changed in the
  // above case, so the NSViewFrameDidChangeNotification registered below will
  // never fire.
  if (extensionFrame.size.height > 0.0) {
    NSSize infoBarSize = [[self view] frame].size;
    infoBarSize.height = extensionFrame.size.height + kBottomBorderHeightPx;
    [[self view] setFrameSize:infoBarSize];
    [infoBarView_ setFrameSize:infoBarSize];
  }

  [self adjustWidthToFitWindow];

  // These two notification handlers are here to ensure the width of the
  // native extension view is the same as the browser window's width and that
  // the parent infobar view matches the height of the extension's native view.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(extensionViewFrameChanged)
             name:NSViewFrameDidChangeNotification
           object:extensionView_];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(adjustWidthToFitWindow)
             name:NSWindowDidResizeNotification
           object:window_];
}

- (void)extensionViewFrameChanged {
  [self adjustWidthToFitWindow];

  AnimatableView* view = [self animatableView];
  NSRect infoBarFrame = [view frame];
  CGFloat newHeight = NSHeight([extensionView_ frame]) + kBottomBorderHeightPx;
  [infoBarView_ setPostsFrameChangedNotifications:NO];
  infoBarFrame.size.height = newHeight;
  [infoBarView_ setFrame:infoBarFrame];
  [infoBarView_ setPostsFrameChangedNotifications:YES];
  [view animateToNewHeight:newHeight duration:kAnimationDuration];
}

- (void)adjustWidthToFitWindow {
  [extensionView_ setPostsFrameChangedNotifications:NO];
  NSSize extensionViewSize = [extensionView_ frame].size;
  extensionViewSize.width = NSWidth([window_ frame]);
  [extensionView_ setFrameSize:extensionViewSize];
  [extensionView_ setPostsFrameChangedNotifications:YES];
}

@end

InfoBar* ExtensionInfoBarDelegate::CreateInfoBar() {
  NSWindow* window = [(NSView*)tab_contents_->GetContentNativeView() window];
  ExtensionInfoBarController* controller =
      [[ExtensionInfoBarController alloc] initWithDelegate:this
                                                    window:window];
  return new InfoBar(controller);
}
