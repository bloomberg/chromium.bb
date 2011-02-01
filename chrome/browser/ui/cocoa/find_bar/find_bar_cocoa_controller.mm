// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#import "chrome/browser/ui/cocoa/find_bar/find_bar_cocoa_controller.h"
#import "chrome/browser/ui/cocoa/find_bar/find_bar_bridge.h"
#import "chrome/browser/ui/cocoa/find_bar/find_bar_text_field.h"
#import "chrome/browser/ui/cocoa/find_bar/find_bar_text_field_cell.h"
#import "chrome/browser/ui/cocoa/find_pasteboard.h"
#import "chrome/browser/ui/cocoa/focus_tracker.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#import "third_party/GTM/AppKit/GTMNSAnimation+Duration.h"

namespace {
const float kFindBarOpenDuration = 0.2;
const float kFindBarCloseDuration = 0.15;
}

@interface FindBarCocoaController (PrivateMethods)
// Returns the appropriate frame for a hidden find bar.
- (NSRect)hiddenFindBarFrame;

// Sets the frame of |findBarView_|.  |duration| is ignored if |animate| is NO.
- (void)setFindBarFrame:(NSRect)endFrame
                animate:(BOOL)animate
               duration:(float)duration;

// Optionally stops the current search, puts |text| into the find bar, and
// enables the buttons, but doesn't start a new search for |text|.
- (void)prepopulateText:(NSString*)text stopSearch:(BOOL)stopSearch;
@end

@implementation FindBarCocoaController

- (id)init {
  if ((self = [super initWithNibName:@"FindBar"
                              bundle:base::mac::MainAppBundle()])) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(findPboardUpdated:)
               name:kFindPasteboardChangedNotification
             object:[FindPasteboard sharedInstance]];
  }
  return self;
}

- (void)dealloc {
  // All animations should be explicitly stopped by the TabContents before a tab
  // is closed.
  DCHECK(!currentAnimation_.get());
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)setFindBarBridge:(FindBarBridge*)findBarBridge {
  DCHECK(!findBarBridge_);  // should only be called once.
  findBarBridge_ = findBarBridge;
}

- (void)awakeFromNib {
  [findBarView_ setFrame:[self hiddenFindBarFrame]];

  // Stopping the search requires a findbar controller, which isn't valid yet
  // during setup. Furthermore, there is no active search yet anyway.
  [self prepopulateText:[[FindPasteboard sharedInstance] findText]
             stopSearch:NO];
}

- (IBAction)close:(id)sender {
  if (findBarBridge_)
    findBarBridge_->GetFindBarController()->EndFindSession(
        FindBarController::kKeepSelection);
}

- (IBAction)previousResult:(id)sender {
  if (findBarBridge_)
    findBarBridge_->GetFindBarController()->tab_contents()->StartFinding(
        base::SysNSStringToUTF16([findText_ stringValue]),
        false, false);
}

- (IBAction)nextResult:(id)sender {
  if (findBarBridge_)
    findBarBridge_->GetFindBarController()->tab_contents()->StartFinding(
        base::SysNSStringToUTF16([findText_ stringValue]),
        true, false);
}

- (void)findPboardUpdated:(NSNotification*)notification {
  if (suppressPboardUpdateActions_)
    return;
  [self prepopulateText:[[FindPasteboard sharedInstance] findText]
             stopSearch:YES];
}

- (void)positionFindBarViewAtMaxY:(CGFloat)maxY maxWidth:(CGFloat)maxWidth {
  static const CGFloat kRightEdgeOffset = 25;
  NSView* containerView = [self view];
  CGFloat containerHeight = NSHeight([containerView frame]);
  CGFloat containerWidth = NSWidth([containerView frame]);

  // Adjust where we'll actually place the find bar.
  CGFloat maxX = maxWidth - kRightEdgeOffset;
  DLOG_IF(WARNING, maxX < 0) << "Window too narrow for find bar";
  maxY += 1;

  NSRect newFrame = NSMakeRect(maxX - containerWidth, maxY - containerHeight,
                               containerWidth, containerHeight);
  [containerView setFrame:newFrame];
}

// NSControl delegate method.
- (void)controlTextDidChange:(NSNotification *)aNotification {
  if (!findBarBridge_)
    return;

  TabContents* tab_contents =
      findBarBridge_->GetFindBarController()->tab_contents();
  if (!tab_contents)
    return;

  NSString* findText = [findText_ stringValue];
  suppressPboardUpdateActions_ = YES;
  [[FindPasteboard sharedInstance] setFindText:findText];
  suppressPboardUpdateActions_ = NO;

  if ([findText length] > 0) {
    tab_contents->StartFinding(base::SysNSStringToUTF16(findText), true, false);
  } else {
    // The textbox is empty so we reset.
    tab_contents->StopFinding(FindBarController::kClearSelection);
    [self updateUIForFindResult:tab_contents->find_result()
                       withText:string16()];
  }
}

// NSControl delegate method
- (BOOL)control:(NSControl*)control
    textView:(NSTextView*)textView
    doCommandBySelector:(SEL)command {
  if (command == @selector(insertNewline:)) {
    // Pressing Return
    NSEvent* event = [NSApp currentEvent];

    if ([event modifierFlags] & NSShiftKeyMask)
      [previousButton_ performClick:nil];
    else
      [nextButton_ performClick:nil];

    return YES;
  } else if (command == @selector(insertLineBreak:)) {
    // Pressing Ctrl-Return
    if (findBarBridge_) {
      findBarBridge_->GetFindBarController()->EndFindSession(
          FindBarController::kActivateSelection);
    }
    return YES;
  } else if (command == @selector(pageUp:) ||
             command == @selector(pageUpAndModifySelection:) ||
             command == @selector(scrollPageUp:) ||
             command == @selector(pageDown:) ||
             command == @selector(pageDownAndModifySelection:) ||
             command == @selector(scrollPageDown:) ||
             command == @selector(scrollToBeginningOfDocument:) ||
             command == @selector(scrollToEndOfDocument:) ||
             command == @selector(moveUp:) ||
             command == @selector(moveDown:)) {
    TabContents* contents =
        findBarBridge_->GetFindBarController()->tab_contents();
    if (!contents)
      return NO;

    // Sanity-check to make sure we got a keyboard event.
    NSEvent* event = [NSApp currentEvent];
    if ([event type] != NSKeyDown && [event type] != NSKeyUp)
      return NO;

    // Forward the event to the renderer.
    // TODO(rohitrao): Should this call -[BaseView keyEvent:]?  Is there code in
    // that function that we want to keep or avoid? Calling
    // |ForwardKeyboardEvent()| directly ignores edit commands, which breaks
    // cmd-up/down if we ever decide to include |moveToBeginningOfDocument:| in
    // the list above.
    RenderViewHost* render_view_host = contents->render_view_host();
    render_view_host->ForwardKeyboardEvent(NativeWebKeyboardEvent(event));
    return YES;
  }

  return NO;
}

// Methods from FindBar
- (void)showFindBar:(BOOL)animate {
  // Save the currently-focused view.  |findBarView_| is in the view
  // hierarchy by now.  showFindBar can be called even when the
  // findbar is already open, so do not overwrite an already saved
  // view.
  if (!focusTracker_.get())
    focusTracker_.reset(
        [[FocusTracker alloc] initWithWindow:[findBarView_ window]]);

  // Animate the view into place.
  NSRect frame = [findBarView_ frame];
  frame.origin = NSMakePoint(0, 0);
  [self setFindBarFrame:frame animate:animate duration:kFindBarOpenDuration];
}

- (void)hideFindBar:(BOOL)animate {
  NSRect frame = [self hiddenFindBarFrame];
  [self setFindBarFrame:frame animate:animate duration:kFindBarCloseDuration];
}

- (void)stopAnimation {
  if (currentAnimation_.get()) {
    [currentAnimation_ stopAnimation];
    currentAnimation_.reset(nil);
  }
}

- (void)setFocusAndSelection {
  [[findText_ window] makeFirstResponder:findText_];

  // Enable the buttons if the find text is non-empty.
  BOOL buttonsEnabled = ([[findText_ stringValue] length] > 0) ? YES : NO;
  [previousButton_ setEnabled:buttonsEnabled];
  [nextButton_ setEnabled:buttonsEnabled];
}

- (void)restoreSavedFocus {
  if (!(focusTracker_.get() &&
        [focusTracker_ restoreFocusInWindow:[findBarView_ window]])) {
    // Fall back to giving focus to the tab contents.
    findBarBridge_->GetFindBarController()->tab_contents()->Focus();
  }
  focusTracker_.reset(nil);
}

- (void)setFindText:(NSString*)findText {
  [findText_ setStringValue:findText];

  // Make sure the text in the find bar always ends up in the find pasteboard
  // (and, via notifications, in the other find bars too).
  [[FindPasteboard sharedInstance] setFindText:findText];
}

- (void)clearResults:(const FindNotificationDetails&)results {
  // Just call updateUIForFindResult, which will take care of clearing
  // the search text and the results label.
  [self updateUIForFindResult:results withText:string16()];
}

- (void)updateUIForFindResult:(const FindNotificationDetails&)result
                     withText:(const string16&)findText {
  // If we don't have any results and something was passed in, then
  // that means someone pressed Cmd-G while the Find box was
  // closed. In that case we need to repopulate the Find box with what
  // was passed in.
  if ([[findText_ stringValue] length] == 0 && !findText.empty()) {
    [findText_ setStringValue:base::SysUTF16ToNSString(findText)];
    [findText_ selectText:self];
  }

  // Make sure Find Next and Find Previous are enabled if we found any matches.
  BOOL buttonsEnabled = result.number_of_matches() > 0 ? YES : NO;
  [previousButton_ setEnabled:buttonsEnabled];
  [nextButton_ setEnabled:buttonsEnabled];

  // Update the results label.
  BOOL validRange = result.active_match_ordinal() != -1 &&
                    result.number_of_matches() != -1;
  NSString* searchString = [findText_ stringValue];
  if ([searchString length] > 0 && validRange) {
    [[findText_ findBarTextFieldCell]
        setActiveMatch:result.active_match_ordinal()
                    of:result.number_of_matches()];
  } else {
    // If there was no text entered, we don't show anything in the results area.
    [[findText_ findBarTextFieldCell] clearResults];
  }

  [findText_ resetFieldEditorFrameIfNeeded];

  // If we found any results, reset the focus tracker, so we always
  // restore focus to the tab contents.
  if (result.number_of_matches() > 0)
    focusTracker_.reset(nil);
}

- (BOOL)isFindBarVisible {
  // Find bar is visible if any part of it is on the screen.
  return NSIntersectsRect([[self view] bounds], [findBarView_ frame]);
}

- (BOOL)isFindBarAnimating {
  return (currentAnimation_.get() != nil);
}

// NSAnimation delegate methods.
- (void)animationDidEnd:(NSAnimation*)animation {
  // Autorelease the animation (cannot use release because the animation object
  // is still on the stack.
  DCHECK(animation == currentAnimation_.get());
  [currentAnimation_.release() autorelease];

  // If the find bar is not visible, make it actually hidden, so it'll no longer
  // respond to key events.
  [findBarView_ setHidden:![self isFindBarVisible]];
}

@end

@implementation FindBarCocoaController (PrivateMethods)

- (NSRect)hiddenFindBarFrame {
  NSRect frame = [findBarView_ frame];
  NSRect containerBounds = [[self view] bounds];
  frame.origin = NSMakePoint(NSMinX(containerBounds), NSMaxY(containerBounds));
  return frame;
}

- (void)setFindBarFrame:(NSRect)endFrame
                animate:(BOOL)animate
               duration:(float)duration {
  // Save the current frame.
  NSRect startFrame = [findBarView_ frame];

  // Stop any existing animations.
  [currentAnimation_ stopAnimation];

  if (!animate) {
    [findBarView_ setFrame:endFrame];
    [findBarView_ setHidden:![self isFindBarVisible]];
    currentAnimation_.reset(nil);
    return;
  }

  // If animating, ensure that the find bar is not hidden. Hidden status will be
  // updated at the end of the animation.
  [findBarView_ setHidden:NO];

  // Reset the frame to what was saved above.
  [findBarView_ setFrame:startFrame];
  NSDictionary* dict = [NSDictionary dictionaryWithObjectsAndKeys:
      findBarView_, NSViewAnimationTargetKey,
      [NSValue valueWithRect:endFrame], NSViewAnimationEndFrameKey, nil];

  currentAnimation_.reset(
      [[NSViewAnimation alloc]
        initWithViewAnimations:[NSArray arrayWithObjects:dict, nil]]);
  [currentAnimation_ gtm_setDuration:duration
                           eventMask:NSLeftMouseUpMask];
  [currentAnimation_ setDelegate:self];
  [currentAnimation_ startAnimation];
}

- (void)prepopulateText:(NSString*)text stopSearch:(BOOL)stopSearch{
  [self setFindText:text];

  // End the find session, hide the "x of y" text and disable the
  // buttons, but do not close the find bar or raise the window here.
  if (stopSearch && findBarBridge_) {
    TabContents* contents =
        findBarBridge_->GetFindBarController()->tab_contents();
    if (contents) {
      contents->StopFinding(FindBarController::kClearSelection);
      findBarBridge_->ClearResults(contents->find_result());
    }
  }

  // Has to happen after |ClearResults()| above.
  BOOL buttonsEnabled = [text length] > 0 ? YES : NO;
  [previousButton_ setEnabled:buttonsEnabled];
  [nextButton_ setEnabled:buttonsEnabled];
}

@end
