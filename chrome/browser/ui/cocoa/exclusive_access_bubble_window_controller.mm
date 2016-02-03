// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/logging.h"  // for NOTREACHED()
#include "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#import "chrome/browser/ui/cocoa/exclusive_access_bubble_window_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/tabs/tab_window_controller.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_type.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extension_registry.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMNSAnimation+Duration.h"
#include "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/accelerators/platform_accelerator_cocoa.h"
#import "ui/base/cocoa/controls/hyperlink_text_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/strings/grit/ui_strings.h"

namespace {
const float kInitialDelay = 3.8;
const float kHideDuration = 0.7;
}  // namespace

@interface OneClickHyperlinkTextView : HyperlinkTextView
@end
@implementation OneClickHyperlinkTextView
- (BOOL)acceptsFirstMouse:(NSEvent*)event {
  return YES;
}
@end

@interface ExclusiveAccessBubbleWindowController (PrivateMethods)
// Sets |exitLabel_| based on |exitLabelPlaceholder_|,
// sets |exitLabelPlaceholder_| to nil,
// sets |denyButton_| text based on |bubbleType_|.
- (void)initializeLabelAndButton;

- (NSString*)getLabelText;

- (void)hideSoon;

// Returns the Accelerator for the Toggle Fullscreen menu item.
+ (scoped_ptr<ui::PlatformAcceleratorCocoa>)acceleratorForToggleFullscreen;

// Returns a string representation fit for display of
// +acceleratorForToggleFullscreen.
+ (NSString*)keyCommandString;

+ (NSString*)keyCombinationForAccelerator:
        (const ui::PlatformAcceleratorCocoa&)item;
@end

@implementation ExclusiveAccessBubbleWindowController

- (id)initWithOwner:(NSWindowController*)owner
    exclusive_access_manager:(ExclusiveAccessManager*)exclusive_access_manager
                     profile:(Profile*)profile
                         url:(const GURL&)url
                  bubbleType:(ExclusiveAccessBubbleType)bubbleType {
  NSString* nibPath =
      [base::mac::FrameworkBundle() pathForResource:@"ExclusiveAccessBubble"
                                             ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibPath owner:self])) {
    exclusive_access_manager_ = exclusive_access_manager;
    profile_ = profile;
    owner_ = owner;
    url_ = url;
    bubbleType_ = bubbleType;
    // Mouse lock expects mouse events to reach the main window immediately.
    // Make the bubble transparent for mouse events if mouse lock is enabled.
    if (bubbleType_ ==
            EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_MOUSELOCK_EXIT_INSTRUCTION ||
        bubbleType_ == EXCLUSIVE_ACCESS_BUBBLE_TYPE_MOUSELOCK_EXIT_INSTRUCTION)
      [[self window] setIgnoresMouseEvents:YES];
  }
  return self;
}

- (void)allow:(id)sender {
  // The mouselock code expects that mouse events reach the main window
  // immediately, but the cursor is still over the bubble, which eats the
  // mouse events. Make the bubble transparent for mouse events.
  if (bubbleType_ ==
          EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_MOUSELOCK_BUTTONS ||
      bubbleType_ == EXCLUSIVE_ACCESS_BUBBLE_TYPE_MOUSELOCK_BUTTONS)
    [[self window] setIgnoresMouseEvents:YES];

  DCHECK(exclusive_access_bubble::ShowButtonsForType(bubbleType_));
  exclusive_access_manager_->OnAcceptExclusiveAccessPermission();
}

- (void)deny:(id)sender {
  DCHECK(exclusive_access_bubble::ShowButtonsForType(bubbleType_));
  exclusive_access_manager_->OnDenyExclusiveAccessPermission();
}

- (void)showButtons:(BOOL)show {
  [allowButton_ setHidden:!show];
  [denyButton_ setHidden:!show];
  [exitLabel_ setHidden:show];
}

// We want this to be a child of a browser window.  addChildWindow:
// (called from this function) will bring the window on-screen;
// unfortunately, [NSWindowController showWindow:] will also bring it
// on-screen (but will cause unexpected changes to the window's
// position).  We cannot have an addChildWindow: and a subsequent
// showWindow:. Thus, we have our own version.
- (void)showWindow {
  // Completes nib load.
  InfoBubbleWindow* info_bubble = static_cast<InfoBubbleWindow*>([self window]);
  [info_bubble setInfoBubbleCanBecomeKeyWindow:NO];
  if (!exclusive_access_bubble::ShowButtonsForType(bubbleType_)) {
    [self hideSoon];
  }
  [tweaker_ tweakUI:info_bubble];
  [[owner_ window] addChildWindow:info_bubble ordered:NSWindowAbove];

  if ([owner_ respondsToSelector:@selector(layoutSubviews)])
    [(id)owner_ layoutSubviews];

  [info_bubble orderFront:self];
}

- (void)awakeFromNib {
  DCHECK([[self window] isKindOfClass:[InfoBubbleWindow class]]);
  [messageLabel_ setStringValue:[self getLabelText]];
  [self initializeLabelAndButton];
}

- (void)positionInWindowAtTop:(CGFloat)maxY {
  NSRect windowFrame = [self window].frame;
  NSRect ownerWindowFrame = [owner_ window].frame;
  NSPoint origin;
  origin.x = ownerWindowFrame.origin.x +
             (int)(NSWidth(ownerWindowFrame) / 2 - NSWidth(windowFrame) / 2);
  origin.y = ownerWindowFrame.origin.y + maxY - NSHeight(windowFrame);
  [[self window] setFrameOrigin:origin];
}

// Called when someone clicks on the embedded link.
- (BOOL)textView:(NSTextView*)textView
    clickedOnLink:(id)link
          atIndex:(NSUInteger)charIndex {
  exclusive_access_manager_->fullscreen_controller()
      ->ExitExclusiveAccessToPreviousState();
  return YES;
}

- (void)hideTimerFired:(NSTimer*)timer {
  // This might fire racily for buttoned bubbles, even though the timer is
  // cancelled for them. Explicitly check for this case.
  if (exclusive_access_bubble::ShowButtonsForType(bubbleType_))
    return;

  [NSAnimationContext beginGrouping];
  [[NSAnimationContext currentContext]
      gtm_setDuration:kHideDuration
            eventMask:NSLeftMouseUpMask | NSLeftMouseDownMask];
  [[[self window] animator] setAlphaValue:0.0];
  [NSAnimationContext endGrouping];
}

- (void)animationDidEnd:(NSAnimation*)animation {
  if (animation == hideAnimation_.get()) {
    hideAnimation_.reset();
  }
}

- (void)closeImmediately {
  // Without this, quitting fullscreen with esc will let the bubble reappear
  // once the "exit fullscreen" animation is done on lion.
  InfoBubbleWindow* infoBubble = static_cast<InfoBubbleWindow*>([self window]);
  [[infoBubble parentWindow] removeChildWindow:infoBubble];
  [hideAnimation_.get() stopAnimation];
  [hideTimer_ invalidate];
  [infoBubble setAllowedAnimations:info_bubble::kAnimateNone];
  [self close];
}

- (void)dealloc {
  [hideAnimation_.get() stopAnimation];
  [hideTimer_ invalidate];
  [super dealloc];
}

@end

@implementation ExclusiveAccessBubbleWindowController (PrivateMethods)

- (void)initializeLabelAndButton {
  // Replace the label placeholder NSTextField with the real label NSTextView.
  // The former doesn't show links in a nice way, but the latter can't be added
  // in IB without a containing scroll view, so create the NSTextView
  // programmatically.
  exitLabel_.reset([[OneClickHyperlinkTextView alloc]
      initWithFrame:[exitLabelPlaceholder_ frame]]);
  [exitLabel_.get()
      setAutoresizingMask:[exitLabelPlaceholder_ autoresizingMask]];
  [exitLabel_.get() setHidden:[exitLabelPlaceholder_ isHidden]];
  [[exitLabelPlaceholder_ superview] replaceSubview:exitLabelPlaceholder_
                                               with:exitLabel_.get()];
  exitLabelPlaceholder_ = nil;  // Now released.
  [exitLabel_.get() setDelegate:self];

  NSString* exitLinkText;
  NSString* exitLinkedText;
  if (bubbleType_ ==
          EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_MOUSELOCK_EXIT_INSTRUCTION ||
      bubbleType_ == EXCLUSIVE_ACCESS_BUBBLE_TYPE_MOUSELOCK_EXIT_INSTRUCTION) {
    exitLinkText = @"";
    exitLinkedText =
        [@" " stringByAppendingString:l10n_util::GetNSStringF(
                                      IDS_FULLSCREEN_PRESS_ESC_TO_EXIT_SENTENCE,
                                      l10n_util::GetStringUTF16(
                                          IDS_APP_ESC_KEY))];
  } else {
    exitLinkText = l10n_util::GetNSString(IDS_EXIT_FULLSCREEN_MODE);
    NSString* messageText = l10n_util::GetNSStringF(
                                   IDS_EXIT_FULLSCREEN_MODE_ACCELERATOR,
                                   l10n_util::GetStringUTF16(IDS_APP_ESC_KEY));
    exitLinkedText =
        [NSString stringWithFormat:@"%@ %@", exitLinkText, messageText];
  }

  NSFont* font = [NSFont
      systemFontOfSize:[NSFont
                           systemFontSizeForControlSize:NSRegularControlSize]];
  [exitLabel_.get() setMessage:exitLinkedText
                      withFont:font
                  messageColor:[NSColor blackColor]];
  if ([exitLinkText length] != 0) {
    [exitLabel_.get() addLinkRange:NSMakeRange(0, [exitLinkText length])
                           withURL:nil
                         linkColor:[NSColor blueColor]];
  }
  [exitLabel_.get() setAlignment:NSRightTextAlignment];

  NSRect labelFrame = [exitLabel_ frame];

  // NSTextView's sizeToFit: method seems to enjoy wrapping lines. Temporarily
  // set the size large to force it not to.
  NSRect windowFrame = [[self window] frame];
  [exitLabel_ setFrameSize:windowFrame.size];
  NSLayoutManager* layoutManager = [exitLabel_ layoutManager];
  NSTextContainer* textContainer = [exitLabel_ textContainer];
  [layoutManager ensureLayoutForTextContainer:textContainer];
  NSRect textFrame = [layoutManager usedRectForTextContainer:textContainer];

  textFrame.size.width = ceil(NSWidth(textFrame));
  labelFrame.origin.x += NSWidth(labelFrame) - NSWidth(textFrame);
  labelFrame.size = textFrame.size;
  [exitLabel_ setFrame:labelFrame];

  // Update the title of |allowButton_| and |denyButton_| according to the
  // current |bubbleType_|, or show no button at all.
  if (exclusive_access_bubble::ShowButtonsForType(bubbleType_)) {
    NSString* denyButtonText =
      SysUTF16ToNSString(
        exclusive_access_bubble::GetDenyButtonTextForType(bubbleType_));
    [denyButton_ setTitle:denyButtonText];
    NSString* allowButtonText = SysUTF16ToNSString(
        exclusive_access_bubble::GetAllowButtonTextForType(bubbleType_, url_));
    [allowButton_ setTitle:allowButtonText];
  } else {
    [self showButtons:NO];
  }
}

- (NSString*)getLabelText {
  if (bubbleType_ == EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE)
    return @"";
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_);
  return SysUTF16ToNSString(exclusive_access_bubble::GetLabelTextForType(
      bubbleType_, url_, registry));
}

// This looks at the Main Menu and determines what the user has set as the
// key combination for quit. It then gets the modifiers and builds an object
// to hold the data.
+ (scoped_ptr<ui::PlatformAcceleratorCocoa>)acceleratorForToggleFullscreen {
  NSMenu* mainMenu = [NSApp mainMenu];
  // Get the application menu (i.e. Chromium).
  for (NSMenuItem* menu in [mainMenu itemArray]) {
    for (NSMenuItem* item in [[menu submenu] itemArray]) {
      // Find the toggle presentation mode item.
      if ([item tag] == IDC_PRESENTATION_MODE) {
        return scoped_ptr<ui::PlatformAcceleratorCocoa>(
            new ui::PlatformAcceleratorCocoa([item keyEquivalent],
                                             [item keyEquivalentModifierMask]));
      }
    }
  }
  // Default to Cmd+Shift+F.
  return scoped_ptr<ui::PlatformAcceleratorCocoa>(
      new ui::PlatformAcceleratorCocoa(@"f",
                                       NSCommandKeyMask | NSShiftKeyMask));
}

// This looks at the Main Menu and determines what the user has set as the
// key combination for quit. It then gets the modifiers and builds a string
// to display them.
+ (NSString*)keyCommandString {
  scoped_ptr<ui::PlatformAcceleratorCocoa> accelerator(
      [[self class] acceleratorForToggleFullscreen]);
  return [[self class] keyCombinationForAccelerator:*accelerator];
}

+ (NSString*)keyCombinationForAccelerator:
        (const ui::PlatformAcceleratorCocoa&)item {
  NSMutableString* string = [NSMutableString string];
  NSUInteger modifiers = item.modifier_mask();

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
      [[NSTimer scheduledTimerWithTimeInterval:kInitialDelay
                                        target:self
                                      selector:@selector(hideTimerFired:)
                                      userInfo:nil
                                       repeats:NO] retain]);
}

@end
