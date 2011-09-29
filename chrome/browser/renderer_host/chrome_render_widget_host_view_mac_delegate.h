// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_WIDGET_HOST_VIEW_DELEGATE_H_
#define CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_WIDGET_HOST_VIEW_DELEGATE_H_
#pragma once

#import <Cocoa/Cocoa.h>

#import "base/memory/scoped_ptr.h"
#import "content/browser/renderer_host/render_widget_host_view_mac_delegate.h"

class RenderWidgetHost;

namespace ChromeRenderWidgetHostViewMacDelegateInternal {
class SpellCheckRenderViewObserver;
}

@interface ChromeRenderWidgetHostViewMacDelegate
    : NSObject<RenderWidgetHostViewMacDelegate> {
 @private
  RenderWidgetHost* render_widget_host_;  // weak
  scoped_ptr<ChromeRenderWidgetHostViewMacDelegateInternal::
      SpellCheckRenderViewObserver> spelling_observer_;

  // If the viewport is scrolled all the way to the left or right.
  // Used for history swiping.
  BOOL isPinnedLeft_;
  BOOL isPinnedRight_;

  // If the main frame has a horizontal scrollbar.
  // Used for history swiping.
  BOOL hasHorizontalScrollbar_;

  // If a scroll event came back unhandled from the renderer. Set to |NO| at
  // the start of a scroll gesture, and then to |YES| if a scroll event comes
  // back unhandled from the renderer.
  // Used for history swiping.
  BOOL gotUnhandledWheelEvent_;

  // Cumulative scroll delta since scroll gesture start. Only valid during
  // scroll gesture handling. Used for history swiping.
  NSSize totalScrollDelta_;

  // Used for continuous spell checking.
  BOOL spellcheck_enabled_;
  BOOL spellcheck_checked_;
}

- (id)initWithRenderWidgetHost:(RenderWidgetHost*)render_widget_host;

- (void)viewGone:(NSView*)view;
- (BOOL)handleEvent:(NSEvent*)event;
- (void)gotUnhandledWheelEvent;
- (void)scrollOffsetPinnedToLeft:(BOOL)left toRight:(BOOL)right;
- (void)setHasHorizontalScrollbar:(BOOL)has_horizontal_scrollbar;
- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item
                      isValidItem:(BOOL*)valid;

@end

#endif  // CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_WIDGET_HOST_VIEW_DELEGATE_H_
