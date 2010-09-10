// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "speech_input_window_controller.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/cocoa/info_bubble_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#import "skia/ext/skia_utils_mac.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"

const int kBubbleControlVerticalSpacing = 10;  // Space between controls.
const int kBubbleHorizontalMargin = 15;  // Space on either sides of controls.

@interface SpeechInputWindowController (Private)
- (NSSize)calculateContentSize;
- (void)layout:(NSSize)size;
@end

@implementation SpeechInputWindowController

- (id)initWithParentWindow:(NSWindow*)parentWindow
                  delegate:(SpeechInputBubbleDelegate*)delegate
              anchoredAt:(NSPoint)anchoredAt {
  anchoredAt.x -= info_bubble::kBubbleArrowXOffset +
                  info_bubble::kBubbleArrowWidth / 2.0;
  if ((self = [super initWithWindowNibPath:@"SpeechInputBubble"
                              parentWindow:parentWindow
                                anchoredAt:anchoredAt])) {
    DCHECK(delegate);
    delegate_ = delegate;

    [self showWindow:nil];
  }
  return self;
}

- (void)awakeFromNib {
  NSWindow* window = [self window];
  [[self bubble] setArrowLocation:info_bubble::kTopLeft];
  NSImage* icon = ResourceBundle::GetSharedInstance().GetNSImageNamed(
      IDR_SPEECH_INPUT_RECORDING);
  [iconImage_ setImage:icon];
  [iconImage_ setNeedsDisplay:YES];

  NSSize newSize = [self calculateContentSize];
  [[self bubble] setFrameSize:newSize];
  NSSize windowDelta = NSMakeSize(
      newSize.width - NSWidth([[window contentView] bounds]),
      newSize.height - NSHeight([[window contentView] bounds]));
  windowDelta = [[window contentView] convertSize:windowDelta toView:nil];
  NSRect newFrame = [window frame];
  newFrame.size.width += windowDelta.width;
  newFrame.size.height += windowDelta.height;
  [window setFrame:newFrame display:NO];

  [self layout:newSize];  // Layout all the child controls.
}

- (IBAction)cancel:(id)sender {
  delegate_->InfoBubbleButtonClicked(SpeechInputBubble::BUTTON_CANCEL);
}

// Calculate the window dimensions to reflect the sum height and max width of
// all controls, with appropriate spacing between and around them. The returned
// size is in view coordinates.
- (NSSize)calculateContentSize {
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:cancelButton_];
  NSSize size = [cancelButton_ bounds].size;
  int newHeight = size.height + kBubbleControlVerticalSpacing;
  int newWidth = size.width;

  NSImage* icon = ResourceBundle::GetSharedInstance().GetNSImageNamed(
      IDR_SPEECH_INPUT_RECORDING);
  size = [icon size];
  newHeight += size.height + kBubbleControlVerticalSpacing;
  if (newWidth < size.width)
    newWidth = size.width;

  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:
      instructionLabel_];
  size = [instructionLabel_ bounds].size;
  newHeight += size.height;
  if (newWidth < size.width)
    newWidth = size.width;

  return NSMakeSize(newWidth + 2 * kBubbleHorizontalMargin,
                    newHeight + 2 * kBubbleControlVerticalSpacing);
}

// Position the controls within the given content area bounds.
- (void)layout:(NSSize)size {
  int y = kBubbleControlVerticalSpacing;

  NSRect rect = [cancelButton_ bounds];
  rect.origin.x = (size.width - rect.size.width) / 2;
  rect.origin.y = y;
  [cancelButton_ setFrame:rect];
  y += rect.size.height + kBubbleControlVerticalSpacing;

  rect = [iconImage_ bounds];
  rect.origin.x = (size.width - rect.size.width) / 2;
  rect.origin.y = y;
  [iconImage_ setFrame:rect];
  y += rect.size.height + kBubbleControlVerticalSpacing;

  rect = [instructionLabel_ bounds];
  rect.origin.x = (size.width - rect.size.width) / 2;
  rect.origin.y = y;
  [instructionLabel_ setFrame:rect];
}

- (void)didStartRecognition {
  NSImage* icon = ResourceBundle::GetSharedInstance().GetNSImageNamed(
      IDR_SPEECH_INPUT_PROCESSING);
  [iconImage_ setImage:icon];
  [iconImage_ setNeedsDisplay:YES];
}

- (void)windowWillClose:(NSNotification*)notification {
  delegate_->InfoBubbleFocusChanged();
}

@end  // implementation SpeechInputWindowController
