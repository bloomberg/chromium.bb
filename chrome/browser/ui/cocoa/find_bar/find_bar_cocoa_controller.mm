// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/find_bar/find_bar_cocoa_controller.h"
#import "chrome/browser/ui/cocoa/find_bar/find_bar_bridge.h"
#import "chrome/browser/ui/cocoa/find_bar/find_bar_text_field.h"
#import "chrome/browser/ui/cocoa/find_bar/find_bar_text_field_cell.h"
#import "chrome/browser/ui/cocoa/find_pasteboard.h"
#import "chrome/browser/ui/cocoa/focus_tracker.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#import "third_party/GTM/AppKit/GTMNSAnimation+Duration.h"

const float kFindBarOpenDuration = 0.2;
const float kFindBarCloseDuration = 0.15;
const float kFindBarMoveDuration = 0.15;
const float kRightEdgeOffset = 25;

@interface FindBarCocoaController (PrivateMethods) <NSAnimationDelegate>
// Returns the appropriate frame for a hidden find bar.
- (NSRect)hiddenFindBarFrame;

// Animates the given |view| to the given |endFrame| within |duration| seconds.
// Returns a new NSViewAnimation.
- (NSViewAnimation*)createAnimationForView:(NSView*)view
                                   toFrame:(NSRect)endFrame
                                  duration:(float)duration;

// Sets the frame of |findBarView_|.  |duration| is ignored if |animate| is NO.
- (void)setFindBarFrame:(NSRect)endFrame
                animate:(BOOL)animate
               duration:(float)duration;

// Returns the horizontal position the FindBar should use in order to avoid
// overlapping with the current find result, if there's one.
- (float)findBarHorizontalPosition;

// Adjusts the horizontal position if necessary to avoid overlapping with the
// current find result.
- (void)moveFindBarIfNecessary:(BOOL)animate;

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
  DCHECK(!showHideAnimation_.get());
  DCHECK(!moveAnimation_.get());
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)setFindBarBridge:(FindBarBridge*)findBarBridge {
  DCHECK(!findBarBridge_);  // should only be called once.
  findBarBridge_ = findBarBridge;
}

- (void)setBrowserWindowController:(BrowserWindowController*)controller {
  DCHECK(!browserWindowController_); // should only be called once.
  browserWindowController_ = controller;
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
  if (findBarBridge_) {
    FindTabHelper* find_tab_helper = findBarBridge_->
        GetFindBarController()->tab_contents()->find_tab_helper();
    find_tab_helper->StartFinding(
        base::SysNSStringToUTF16([findText_ stringValue]),
        false, false);
  }
}

- (IBAction)nextResult:(id)sender {
  if (findBarBridge_) {
    FindTabHelper* find_tab_helper = findBarBridge_->
        GetFindBarController()->tab_contents()->find_tab_helper();
    find_tab_helper->StartFinding(
        base::SysNSStringToUTF16([findText_ stringValue]),
        true, false);
  }
}

- (void)findPboardUpdated:(NSNotification*)notification {
  if (suppressPboardUpdateActions_)
    return;
  [self prepopulateText:[[FindPasteboard sharedInstance] findText]
             stopSearch:YES];
}

- (void)positionFindBarViewAtMaxY:(CGFloat)maxY maxWidth:(CGFloat)maxWidth {
  NSView* containerView = [self view];
  CGFloat containerHeight = NSHeight([containerView frame]);
  CGFloat containerWidth = NSWidth([containerView frame]);

  // Adjust where we'll actually place the find bar.
  maxY += 1;
  maxY_ = maxY;
  CGFloat x = [self findBarHorizontalPosition];
  NSRect newFrame = NSMakeRect(x, maxY - containerHeight,
                               containerWidth, containerHeight);

  if (moveAnimation_.get() != nil) {
    NSRect frame = [containerView frame];
    [moveAnimation_ stopAnimation];
    // Restore to the X position before the animation was stopped. The Y
    // position is immediately adjusted.
    frame.origin.y = newFrame.origin.y;
    [containerView setFrame:frame];
    moveAnimation_.reset([self createAnimationForView:containerView
                                              toFrame:newFrame
                                             duration:kFindBarMoveDuration]);
  } else {
    [containerView setFrame:newFrame];
  }
}

// NSControl delegate method.
- (void)controlTextDidChange:(NSNotification *)aNotification {
  if (!findBarBridge_)
    return;

  TabContentsWrapper* tab_contents =
      findBarBridge_->GetFindBarController()->tab_contents();
  if (!tab_contents)
    return;
  FindTabHelper* find_tab_helper = tab_contents->find_tab_helper();

  NSString* findText = [findText_ stringValue];
  suppressPboardUpdateActions_ = YES;
  [[FindPasteboard sharedInstance] setFindText:findText];
  suppressPboardUpdateActions_ = NO;

  if ([findText length] > 0) {
    find_tab_helper->
        StartFinding(base::SysNSStringToUTF16(findText), true, false);
  } else {
    // The textbox is empty so we reset.
    find_tab_helper->StopFinding(FindBarController::kClearSelection);
    [self updateUIForFindResult:find_tab_helper->find_result()
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
    TabContentsWrapper* contents =
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

  // The browser window might have changed while the FindBar was hidden.
  // Update its position now.
  [browserWindowController_ layoutSubviews];

  // Move to the correct horizontal position first, to prevent the FindBar
  // from jumping around when switching tabs.
  // Prevent jumping while the FindBar is animating (hiding, then showing) too.
  if (![self isFindBarVisible])
    [self moveFindBarIfNecessary:NO];

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
  if (showHideAnimation_.get()) {
    [showHideAnimation_ stopAnimation];
    showHideAnimation_.reset(nil);
  }
  if (moveAnimation_.get()) {
    [moveAnimation_ stopAnimation];
    moveAnimation_.reset(nil);
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
    findBarBridge_->
        GetFindBarController()->tab_contents()->tab_contents()->Focus();
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

  // Adjust the FindBar position, even when there are no matches (so that it
  // goes back to the default position, if required).
  [self moveFindBarIfNecessary:[self isFindBarVisible]];
}

- (BOOL)isFindBarVisible {
  // Find bar is visible if any part of it is on the screen.
  return NSIntersectsRect([[self view] bounds], [findBarView_ frame]);
}

- (BOOL)isFindBarAnimating {
  return (showHideAnimation_.get() != nil) || (moveAnimation_.get() != nil);
}

// NSAnimation delegate methods.
- (void)animationDidEnd:(NSAnimation*)animation {
  // Autorelease the animations (cannot use release because the animation object
  // is still on the stack.
  if (animation == showHideAnimation_.get()) {
    [showHideAnimation_.release() autorelease];
  } else if (animation == moveAnimation_.get()) {
    [moveAnimation_.release() autorelease];
  } else {
    NOTREACHED();
  }

  // If the find bar is not visible, make it actually hidden, so it'll no longer
  // respond to key events.
  [findBarView_ setHidden:![self isFindBarVisible]];
}

- (gfx::Point)findBarWindowPosition {
  gfx::Rect view_rect(NSRectToCGRect([[self view] frame]));
  // Convert Cocoa coordinates (Y growing up) to Y growing down.
  // Offset from |maxY_|, which represents the content view's top, instead
  // of from the superview, which represents the whole browser window.
  view_rect.set_y(maxY_ - view_rect.bottom());
  return view_rect.origin();
}

@end

@implementation FindBarCocoaController (PrivateMethods)

- (NSRect)hiddenFindBarFrame {
  NSRect frame = [findBarView_ frame];
  NSRect containerBounds = [[self view] bounds];
  frame.origin = NSMakePoint(NSMinX(containerBounds), NSMaxY(containerBounds));
  return frame;
}

- (NSViewAnimation*)createAnimationForView:(NSView*)view
                                   toFrame:(NSRect)endFrame
                                  duration:(float)duration {
  NSDictionary* dict = [NSDictionary dictionaryWithObjectsAndKeys:
      view, NSViewAnimationTargetKey,
      [NSValue valueWithRect:endFrame], NSViewAnimationEndFrameKey, nil];

  NSViewAnimation* animation =
      [[NSViewAnimation alloc]
        initWithViewAnimations:[NSArray arrayWithObjects:dict, nil]];
  [animation gtm_setDuration:duration
                           eventMask:NSLeftMouseUpMask];
  [animation setDelegate:self];
  [animation startAnimation];
  return animation;
}

- (void)setFindBarFrame:(NSRect)endFrame
                animate:(BOOL)animate
               duration:(float)duration {
  // Save the current frame.
  NSRect startFrame = [findBarView_ frame];

  // Stop any existing animations.
  [showHideAnimation_ stopAnimation];

  if (!animate) {
    [findBarView_ setFrame:endFrame];
    [findBarView_ setHidden:![self isFindBarVisible]];
    showHideAnimation_.reset(nil);
    return;
  }

  // If animating, ensure that the find bar is not hidden. Hidden status will be
  // updated at the end of the animation.
  [findBarView_ setHidden:NO];

  // Reset the frame to what was saved above.
  [findBarView_ setFrame:startFrame];

  showHideAnimation_.reset([self createAnimationForView:findBarView_
                                                toFrame:endFrame
                                               duration:duration]);
}

- (float)findBarHorizontalPosition {
  // Get the rect of the FindBar.
  NSView* view = [self view];
  NSRect frame = [view frame];
  gfx::Rect view_rect(NSRectToCGRect(frame));

  if (!findBarBridge_ || !findBarBridge_->GetFindBarController())
    return frame.origin.x;
  TabContentsWrapper* contents =
      findBarBridge_->GetFindBarController()->tab_contents();
  if (!contents)
    return frame.origin.x;

  // Get the size of the container.
  gfx::Rect container_rect(contents->view()->GetContainerSize());

  // Position the FindBar on the top right corner.
  view_rect.set_x(
      container_rect.width() - view_rect.width() - kRightEdgeOffset);
  // Convert from Cocoa coordinates (Y growing up) to Y growing down.
  // Notice that the view frame's Y offset is relative to the whole window,
  // while GetLocationForFindbarView() expects it relative to the
  // content's boundaries. |maxY_| has the correct placement in Cocoa coords,
  // so we just have to invert the Y coordinate.
  view_rect.set_y(maxY_ - view_rect.bottom());

  // Get the rect of the current find result, if there is one.
  const FindNotificationDetails& find_result =
      contents->find_tab_helper()->find_result();
  if (find_result.number_of_matches() == 0)
    return view_rect.x();
  gfx::Rect selection_rect(find_result.selection_rect());

  // Adjust |view_rect| to avoid the |selection_rect| within |container_rect|.
  gfx::Rect new_pos = FindBarController::GetLocationForFindbarView(
      view_rect, container_rect, selection_rect);

  return new_pos.x();
}

- (void)moveFindBarIfNecessary:(BOOL)animate {
  // Don't animate during tests.
  if (FindBarBridge::disable_animations_during_testing_)
    animate = NO;

  NSView* view = [self view];
  NSRect frame = [view frame];
  float x = [self findBarHorizontalPosition];

  if (animate) {
    [moveAnimation_ stopAnimation];
    // Restore to the position before the animation was stopped.
    [view setFrame:frame];
    frame.origin.x = x;
    moveAnimation_.reset([self createAnimationForView:view
                                              toFrame:frame
                                             duration:kFindBarMoveDuration]);
  } else {
    frame.origin.x = x;
    [view setFrame:frame];
  }
}

- (void)prepopulateText:(NSString*)text stopSearch:(BOOL)stopSearch{
  [self setFindText:text];

  // End the find session, hide the "x of y" text and disable the
  // buttons, but do not close the find bar or raise the window here.
  if (stopSearch && findBarBridge_) {
    TabContentsWrapper* contents =
        findBarBridge_->GetFindBarController()->tab_contents();
    if (contents) {
      FindTabHelper* find_tab_helper = contents->find_tab_helper();
      find_tab_helper->StopFinding(FindBarController::kClearSelection);
      findBarBridge_->ClearResults(find_tab_helper->find_result());
    }
  }

  // Has to happen after |ClearResults()| above.
  BOOL buttonsEnabled = [text length] > 0 ? YES : NO;
  [previousButton_ setEnabled:buttonsEnabled];
  [nextButton_ setEnabled:buttonsEnabled];
}

@end
