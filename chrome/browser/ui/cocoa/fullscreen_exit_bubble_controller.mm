// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/logging.h"  // for NOTREACHED()
#include "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#import "chrome/browser/ui/cocoa/animatable_view.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/event_utils.h"
#import "chrome/browser/ui/cocoa/fullscreen_exit_bubble_controller.h"
#import "chrome/browser/ui/cocoa/hyperlink_text_view.h"
#include "grit/generated_resources.h"
#include "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "third_party/GTM/AppKit/GTMNSAnimation+Duration.h"
#include "ui/base/models/accelerator_cocoa.h"
#include "ui/base/l10n/l10n_util_mac.h"

const int kPaddingPx = 8;
const int kInitialDelayMs = 3800;
const int kSlideOutDurationMs = 700;

@interface FullscreenExitBubbleController (PrivateMethods)
// Sets |exitLabel_| based on |exitLabelPlaceholder_|,
// sets |exitLabelPlaceholder_| to nil.
- (void)initializeLabel;

- (void)hideSoon;

// Returns the Accelerator for the Toggle Fullscreen menu item.
+ (ui::AcceleratorCocoa)acceleratorForToggleFullscreen;

// Returns a string representation fit for display of
// +acceleratorForToggleFullscreen.
+ (NSString*)keyCommandString;

+ (NSString*)keyCombinationForAccelerator:(const ui::AcceleratorCocoa&)item;
@end

@implementation FullscreenExitBubbleController

- (id)initWithOwner:(BrowserWindowController*)owner browser:(Browser*)browser {
  if ((self = [super initWithNibName:@"FullscreenExitBubble"
                              bundle:base::mac::MainAppBundle()])) {
    browser_ = browser;
    owner_ = owner;
  }
  return self;
}

- (void)awakeFromNib {
  [self initializeLabel];
  [self hideSoon];
}

- (void)positionInWindowAtTop:(CGFloat)maxY width:(CGFloat)maxWidth {
  NSRect bubbleFrame = [[self view] frame];
  bubbleFrame.origin.x = (int)(maxWidth/2 - NSWidth(bubbleFrame)/2);
  bubbleFrame.origin.y = maxY - NSHeight(bubbleFrame);
  [[self view] setFrame:bubbleFrame];
}

// Called when someone clicks on the embedded link.
- (BOOL) textView:(NSTextView*)textView
    clickedOnLink:(id)link
          atIndex:(NSUInteger)charIndex {
  browser_->ExecuteCommand(IDC_FULLSCREEN);
  return YES;
}

- (void)hideTimerFired:(NSTimer*)timer {
  NSRect endFrame = [[self view] frame];
  endFrame.origin.y += endFrame.size.height;
  endFrame.size.height = 0;
  NSDictionary* dict = [NSDictionary dictionaryWithObjectsAndKeys:
      [self view], NSViewAnimationTargetKey,
      [NSValue valueWithRect:endFrame], NSViewAnimationEndFrameKey, nil];

  NSViewAnimation* animation =
      [[NSViewAnimation alloc]
        initWithViewAnimations:[NSArray arrayWithObjects:dict, nil]];
  [animation gtm_setDuration:kSlideOutDurationMs/1000.0
                   eventMask:NSLeftMouseUpMask];
  [animation setDelegate:self];
  [animation startAnimation];
  hideAnimation_.reset(animation);
}

- (void)animationDidEnd:(NSAnimation*)animation {
  if (animation == hideAnimation_.get()) {
    hideAnimation_.reset();
  }
}

- (AnimatableView*)animatableView {
  return static_cast<AnimatableView*>([self view]);
}

- (void)dealloc {
  [hideAnimation_.get() stopAnimation];
  [hideTimer_ invalidate];
  [super dealloc];
}

@end

@implementation FullscreenExitBubbleController (PrivateMethods)

- (void)initializeLabel {
  // Replace the label placeholder NSTextField with the real label NSTextView.
  // The former doesn't show links in a nice way, but the latter can't be added
  // in IB without a containing scroll view, so create the NSTextView
  // programmatically.
  exitLabel_.reset([[HyperlinkTextView alloc]
      initWithFrame:[exitLabelPlaceholder_ frame]]);
  [exitLabel_.get() setAutoresizingMask:
      [exitLabelPlaceholder_ autoresizingMask]];
  [[exitLabelPlaceholder_ superview]
      replaceSubview:exitLabelPlaceholder_ with:exitLabel_.get()];
  exitLabelPlaceholder_ = nil;  // Now released.
  [exitLabel_.get() setDelegate:self];

  NSString *message = l10n_util::GetNSStringF(IDS_EXIT_FULLSCREEN_MODE,
      base::SysNSStringToUTF16([[self class] keyCommandString]));

  [(HyperlinkTextView*)exitLabel_.get()
        setMessageAndLink:@""
                 withLink:message
                 atOffset:0
                     font:[NSFont systemFontOfSize:18]
             messageColor:[NSColor whiteColor]
                linkColor:[NSColor whiteColor]];

  [exitLabel_.get() sizeToFit];
  NSLayoutManager* layoutManager = [exitLabel_.get() layoutManager];
  NSTextContainer* textContainer = [exitLabel_.get() textContainer];
  [layoutManager ensureLayoutForTextContainer:textContainer];
  NSRect textFrame = [layoutManager usedRectForTextContainer:textContainer];
  NSRect frame = [[self view] frame];
  NSSize textSize = textFrame.size;
  frame.size.width = textSize.width + 2 * kPaddingPx;
  [[self view] setFrame:frame];
  textFrame.origin.x = textFrame.origin.y = kPaddingPx;
  [exitLabel_.get() setFrame:textFrame];
}

// This looks at the Main Menu and determines what the user has set as the
// key combination for quit. It then gets the modifiers and builds an object
// to hold the data.
+ (ui::AcceleratorCocoa)acceleratorForToggleFullscreen {
  NSMenu* mainMenu = [NSApp mainMenu];
  // Get the application menu (i.e. Chromium).
  for (NSMenuItem* menu in [mainMenu itemArray]) {
    for (NSMenuItem* item in [[menu submenu] itemArray]) {
      // Find the toggle presentation mode item.
      if ([item tag] == IDC_PRESENTATION_MODE) {
        return ui::AcceleratorCocoa([item keyEquivalent],
                                    [item keyEquivalentModifierMask]);
      }
    }
  }
  // Default to Cmd+Shift+F.
  return ui::AcceleratorCocoa(@"f", NSCommandKeyMask|NSShiftKeyMask);
}

// This looks at the Main Menu and determines what the user has set as the
// key combination for quit. It then gets the modifiers and builds a string
// to display them.
+ (NSString*)keyCommandString {
  ui::AcceleratorCocoa accelerator =
      [[self class] acceleratorForToggleFullscreen];
  return [[self class] keyCombinationForAccelerator:accelerator];
}

+ (NSString*)keyCombinationForAccelerator:(const ui::AcceleratorCocoa&)item {
  NSMutableString* string = [NSMutableString string];
  NSUInteger modifiers = item.modifiers();

  if (modifiers & NSCommandKeyMask)
    [string appendString:@"\u2318"];
  if (modifiers & NSControlKeyMask)
    [string appendString:@"\u2303"];
  if (modifiers & NSAlternateKeyMask)
    [string appendString:@"\u2325"];
  BOOL isUpperCase = [[NSCharacterSet uppercaseLetterCharacterSet]
      characterIsMember:[item.characters() characterAtIndex:0]];
  if (modifiers & NSShiftKeyMask || isUpperCase)
    [string appendString:@"\u21E7"];

  [string appendString:[item.characters() uppercaseString]];
  return string;
}

- (void)hideSoon {
  hideTimer_.reset(
      [[NSTimer scheduledTimerWithTimeInterval:kInitialDelayMs/1000.0
                                        target:self
                                      selector:@selector(hideTimerFired:)
                                      userInfo:nil
                                       repeats:NO] retain]);
}

@end
