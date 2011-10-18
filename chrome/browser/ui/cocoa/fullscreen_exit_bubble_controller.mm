// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/logging.h"  // for NOTREACHED()
#include "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/event_utils.h"
#import "chrome/browser/ui/cocoa/fullscreen_exit_bubble_controller.h"
#import "chrome/browser/ui/cocoa/hyperlink_text_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "grit/generated_resources.h"
#include "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "third_party/GTM/AppKit/GTMNSAnimation+Duration.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/models/accelerator_cocoa.h"
#include "ui/base/l10n/l10n_util_mac.h"


namespace {
const int kBubbleOffsetY = 10;
const float kInitialDelay = 3.8;
const float kHideDuration = 0.7;
} // namespace

@interface OneClickHyperlinkTextView : HyperlinkTextView
@end
@implementation OneClickHyperlinkTextView
- (BOOL)acceptsFirstMouse:(NSEvent*)event {
  return YES;
}
@end

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

- (id)initWithOwner:(BrowserWindowController*)owner
            browser:(Browser*)browser
                url:(const GURL&)url
         bubbleType:(FullscreenExitBubbleType)bubbleType {
  NSString* nibPath =
      [base::mac::MainAppBundle() pathForResource:@"FullscreenExitBubble"
                                          ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibPath owner:self])) {
    browser_ = browser;
    owner_ = owner;
    url_ = url;
    showButtons_ = bubbleType == FEB_TYPE_FULLSCREEN_BUTTONS;
  }
  return self;
}

- (void)allow:(id)sender {
  [self hideButtons];
  browser_->OnAcceptFullscreenPermission(url_);
  [self hideSoon];
}

- (void)deny:(id)sender {
  browser_->ToggleFullscreenMode(false);
}

- (void)hideButtons {
  [allowButton_ setHidden:YES];
  [denyButton_ setHidden:YES];
  [exitLabel_ setHidden:NO];
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
  [bubble_ setArrowLocation:info_bubble::kNoArrow];
  [info_bubble setCanBecomeKeyWindow:NO];
  if (!showButtons_) {
    [self hideButtons];
    [self hideSoon];
  }
  NSRect windowFrame = [owner_ window].frame;
  [tweaker_ tweakUI:info_bubble];
  [self positionInWindowAtTop:NSHeight(windowFrame) width:NSWidth(windowFrame)];
  [[owner_ window] addChildWindow:info_bubble ordered:NSWindowAbove];

  [info_bubble orderFront:self];
}

- (void)awakeFromNib {
  DCHECK([[self window] isKindOfClass:[InfoBubbleWindow class]]);
  NSString* title =
      l10n_util::GetNSStringF(IDS_FULLSCREEN_INFOBAR_REQUEST_PERMISSION,
          UTF8ToUTF16(url_.host()));
  [messageLabel_ setStringValue:title];
  [self initializeLabel];
}

- (void)positionInWindowAtTop:(CGFloat)maxY width:(CGFloat)maxWidth {
  NSRect windowFrame = [self window].frame;
  NSPoint origin;
  origin.x = (int)(maxWidth/2 - NSWidth(windowFrame)/2);
  origin.y = maxY - NSHeight(windowFrame);
  origin.y -= kBubbleOffsetY;
  [[self window] setFrameOrigin:origin];
}

- (void)updateURL:(const GURL&)url
       bubbleType:(FullscreenExitBubbleType)bubbleType {
  NOTIMPLEMENTED();
}

// Called when someone clicks on the embedded link.
- (BOOL) textView:(NSTextView*)textView
    clickedOnLink:(id)link
          atIndex:(NSUInteger)charIndex {
  browser_->ExecuteCommand(IDC_FULLSCREEN);
  return YES;
}

- (void)hideTimerFired:(NSTimer*)timer {
  [NSAnimationContext beginGrouping];
  [[NSAnimationContext currentContext]
      gtm_setDuration:kHideDuration
            eventMask:NSLeftMouseUpMask|NSLeftMouseDownMask];
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
  [infoBubble setDelayOnClose:NO];
  [self close];
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
  exitLabel_.reset([[OneClickHyperlinkTextView alloc]
      initWithFrame:[exitLabelPlaceholder_ frame]]);
  [exitLabel_.get() setAutoresizingMask:
      [exitLabelPlaceholder_ autoresizingMask]];
  [exitLabel_.get() setHidden:[exitLabelPlaceholder_ isHidden]];
  [[exitLabelPlaceholder_ superview]
      replaceSubview:exitLabelPlaceholder_ with:exitLabel_.get()];
  exitLabelPlaceholder_ = nil;  // Now released.
  [exitLabel_.get() setDelegate:self];

  NSString *message = l10n_util::GetNSStringF(IDS_EXIT_FULLSCREEN_MODE,
      base::SysNSStringToUTF16([[self class] keyCommandString]));

  NSFont* font = [NSFont systemFontOfSize:
      [NSFont systemFontSizeForControlSize:NSRegularControlSize]];
  [(HyperlinkTextView*)exitLabel_.get()
        setMessageAndLink:@""
                 withLink:message
                 atOffset:0
                     font:font
             messageColor:[NSColor blackColor]
                linkColor:[NSColor blueColor]];
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

  labelFrame.origin.x += NSWidth(labelFrame) - NSWidth(textFrame);
  labelFrame.size = textFrame.size;
  [exitLabel_ setFrame:labelFrame];
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
      [[NSTimer scheduledTimerWithTimeInterval:kInitialDelay
                                        target:self
                                      selector:@selector(hideTimerFired:)
                                      userInfo:nil
                                       repeats:NO] retain]);
}

@end
