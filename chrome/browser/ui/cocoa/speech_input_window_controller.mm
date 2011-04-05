// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "speech_input_window_controller.h"

#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/ui/cocoa/info_bubble_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "media/audio/audio_manager.h"
#import "skia/ext/skia_utils_mac.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image.h"

const int kBubbleControlVerticalSpacing = 10;  // Space between controls.
const int kBubbleHorizontalMargin = 5;  // Space on either sides of controls.
const int kInstructionLabelMaxWidth = 150;

@interface SpeechInputWindowController (Private)
- (NSSize)calculateContentSize;
- (void)layout:(NSSize)size;
@end

@implementation SpeechInputWindowController

- (id)initWithParentWindow:(NSWindow*)parentWindow
                  delegate:(SpeechInputBubbleDelegate*)delegate
              anchoredAt:(NSPoint)anchoredAt {
  anchoredAt.y += info_bubble::kBubbleArrowHeight / 2.0;
  if ((self = [super initWithWindowNibPath:@"SpeechInputBubble"
                              parentWindow:parentWindow
                                anchoredAt:anchoredAt])) {
    DCHECK(delegate);
    delegate_ = delegate;
    displayMode_ = SpeechInputBubbleBase::DISPLAY_MODE_WARM_UP;
  }
  return self;
}

- (void)awakeFromNib {
  [super awakeFromNib];
  [[self bubble] setArrowLocation:info_bubble::kTopLeft];
}

- (IBAction)cancel:(id)sender {
  delegate_->InfoBubbleButtonClicked(SpeechInputBubble::BUTTON_CANCEL);
}

- (IBAction)tryAgain:(id)sender {
  delegate_->InfoBubbleButtonClicked(SpeechInputBubble::BUTTON_TRY_AGAIN);
}

- (IBAction)micSettings:(id)sender {
  [[NSWorkspace sharedWorkspace] openFile:
       @"/System/Library/PreferencePanes/Sound.prefPane"];
}

// Calculate the window dimensions to reflect the sum height and max width of
// all controls, with appropriate spacing between and around them. The returned
// size is in view coordinates.
- (NSSize)calculateContentSize {
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:cancelButton_];
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:tryAgainButton_];
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:micSettingsButton_];
  NSSize cancelSize = [cancelButton_ bounds].size;
  NSSize tryAgainSize = [tryAgainButton_ bounds].size;
  CGFloat newHeight = cancelSize.height + kBubbleControlVerticalSpacing;
  CGFloat newWidth = cancelSize.width;
  if (![tryAgainButton_ isHidden])
    newWidth += tryAgainSize.width;

  // The size of the bubble in warm up mode is fixed to be the same as in
  // recording mode, so from warm up it can transition to recording without any
  // UI jank.
  bool isWarmUp = (displayMode_ ==
                   SpeechInputBubbleBase::DISPLAY_MODE_WARM_UP);

  if (![iconImage_ isHidden]) {
    NSSize size = [[iconImage_ image] size];
    if (isWarmUp) {
      NSImage* volumeIcon =
          ResourceBundle::GetSharedInstance().GetNativeImageNamed(
              IDR_SPEECH_INPUT_MIC_EMPTY);
      size = [volumeIcon size];
    }
    newHeight += size.height;
    newWidth = std::max(newWidth, size.width + 2 * kBubbleHorizontalMargin);
  }

  if (![instructionLabel_ isHidden] || isWarmUp) {
    [instructionLabel_ sizeToFit];
    NSSize textSize = [[instructionLabel_ cell] cellSize];
    NSRect boundsRect = NSMakeRect(0, 0, kInstructionLabelMaxWidth,
                                   CGFLOAT_MAX);
    NSSize multiLineSize =
        [[instructionLabel_ cell] cellSizeForBounds:boundsRect];
    if (textSize.width > multiLineSize.width)
      textSize = multiLineSize;
    newHeight += textSize.height + kBubbleControlVerticalSpacing;
    newWidth = std::max(newWidth, textSize.width);
  }

  if (![micSettingsButton_ isHidden]) {
    NSSize size = [micSettingsButton_ bounds].size;
    newHeight += size.height;
    newWidth = std::max(newWidth, size.width);
  }

  return NSMakeSize(newWidth + 2 * kBubbleHorizontalMargin,
                    newHeight + 3 * kBubbleControlVerticalSpacing);
}

// Position the controls within the given content area bounds.
- (void)layout:(NSSize)size {
  int y = kBubbleControlVerticalSpacing;

  NSRect cancelRect = [cancelButton_ bounds];

  if ([tryAgainButton_ isHidden]) {
    cancelRect.origin.x = (size.width - NSWidth(cancelRect)) / 2;
  } else {
    NSRect tryAgainRect = [tryAgainButton_ bounds];
    cancelRect.origin.x = (size.width - NSWidth(cancelRect) -
                           NSWidth(tryAgainRect)) / 2;
    tryAgainRect.origin.x = cancelRect.origin.x + NSWidth(cancelRect);
    tryAgainRect.origin.y = y;
    [tryAgainButton_ setFrame:tryAgainRect];
  }
  cancelRect.origin.y = y;

  if (![cancelButton_ isHidden]) {
    [cancelButton_ setFrame:cancelRect];
    y += NSHeight(cancelRect) + kBubbleControlVerticalSpacing;
  }

  NSRect rect;
  if (![micSettingsButton_ isHidden]) {
    rect = [micSettingsButton_ bounds];
    rect.origin.x = (size.width - NSWidth(rect)) / 2;
    rect.origin.y = y;
    [micSettingsButton_ setFrame:rect];
    y += rect.size.height + kBubbleControlVerticalSpacing;
  }

  if (![instructionLabel_ isHidden]) {
    int spaceForIcon = 0;
    if (![iconImage_ isHidden]) {
      spaceForIcon = [[iconImage_ image] size].height +
                     kBubbleControlVerticalSpacing;
    }

    rect = NSMakeRect(0, y, size.width, size.height - y - spaceForIcon -
                      kBubbleControlVerticalSpacing * 2);
    [instructionLabel_ setFrame:rect];
    y = size.height - spaceForIcon - kBubbleControlVerticalSpacing;
  }

  if (![iconImage_ isHidden]) {
    rect.size = [[iconImage_ image] size];
    // In warm-up mode only the icon gets displayed so center it vertically.
    if (displayMode_ == SpeechInputBubbleBase::DISPLAY_MODE_WARM_UP)
      y = (size.height - rect.size.height) / 2;
    rect.origin.x = (size.width - NSWidth(rect)) / 2;
    rect.origin.y = y;
    [iconImage_ setFrame:rect];
  }
}

- (void)updateLayout:(SpeechInputBubbleBase::DisplayMode)mode
         messageText:(const string16&)messageText
           iconImage:(NSImage*)iconImage {
  // The very first time this method is called, the child views would still be
  // uninitialized and null. So we invoke [self window] first and that sets up
  // the child views properly so we can do the layout calculations below.
  NSWindow* window = [self window];
  displayMode_ = mode;
  BOOL is_message = (mode == SpeechInputBubbleBase::DISPLAY_MODE_MESSAGE);
  BOOL is_recording = (mode == SpeechInputBubbleBase::DISPLAY_MODE_RECORDING);
  BOOL is_warm_up = (mode == SpeechInputBubbleBase::DISPLAY_MODE_WARM_UP);
  [iconImage_ setHidden:is_message];
  [tryAgainButton_ setHidden:!is_message];
  [micSettingsButton_ setHidden:!is_message];
  [instructionLabel_ setHidden:!is_message && !is_recording];
  [cancelButton_ setHidden:is_warm_up];

  // Get the right set of controls to be visible.
  if (is_message) {
    [instructionLabel_ setStringValue:base::SysUTF16ToNSString(messageText)];
  } else {
    [iconImage_ setImage:iconImage];
    [instructionLabel_ setStringValue:l10n_util::GetNSString(
        IDS_SPEECH_INPUT_BUBBLE_HEADING)];
  }

  NSSize newSize = [self calculateContentSize];
  [[self bubble] setFrameSize:newSize];

  NSSize windowDelta = [[window contentView] convertSize:newSize toView:nil];
  NSRect newFrame = [window frame];
  newFrame.origin.y -= windowDelta.height - newFrame.size.height;
  newFrame.size = windowDelta;
  [window setFrame:newFrame display:YES];

  [self layout:newSize];  // Layout all the child controls.
}

- (void)windowWillClose:(NSNotification*)notification {
  delegate_->InfoBubbleFocusChanged();
}

- (void)show {
  [self showWindow:nil];
}

- (void)hide {
  [[self window] orderOut:nil];
}

- (void)setImage:(NSImage*)image {
  [iconImage_ setImage:image];
}

@end  // implementation SpeechInputWindowController
