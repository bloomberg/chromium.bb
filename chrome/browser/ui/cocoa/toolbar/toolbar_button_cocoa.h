// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TOOLBAR_TOOLBAR_BUTTON_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_TOOLBAR_TOOLBAR_BUTTON_COCOA_H_

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/themed_window.h"
#import "ui/gfx/color_utils.h"
#include "ui/gfx/vector_icons_public.h"

enum class ToolbarButtonImageBackgroundStyle {
  HOVER,
  HOVER_THEMED,
  PRESSED,
  PRESSED_THEMED,
};

// NSButton subclass which handles middle mouse clicking.
@interface ToolbarButton : NSButton<ThemedWindowDrawing>  {
 @protected
  // YES when middle mouse clicks should be handled.
  BOOL handleMiddleClick_;
}

// Whether or not to handle the mouse middle click events.
@property(assign, nonatomic) BOOL handleMiddleClick;
// Override point for subclasses to return their icon color.
- (SkColor)iconColor:(BOOL)themeIsDark;
// Sets images for each of the ToolbarButton's states from the specified
// vector icon.
- (void)setImagesFromIconId:(gfx::VectorIconId)iconId;
// Override point for subclasses to set the button's icons.
- (void)resetIcons;
@end

@interface ToolbarButton (ExposedForTesting)
- (BOOL)shouldHandleEvent:(NSEvent*)theEvent;
@end

#endif  // CHROME_BROWSER_UI_COCOA_TOOLBAR_TOOLBAR_BUTTON_COCOA_H_
