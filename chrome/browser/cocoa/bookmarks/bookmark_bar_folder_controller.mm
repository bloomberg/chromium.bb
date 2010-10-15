// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmarks/bookmark_bar_folder_controller.h"

#include "base/mac_util.h"
#include "base/nsimage_cache_mac.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#import "chrome/browser/cocoa/bookmarks/bookmark_bar_constants.h"
#import "chrome/browser/cocoa/bookmarks/bookmark_bar_controller.h"
#import "chrome/browser/cocoa/bookmarks/bookmark_bar_folder_button_cell.h"
#import "chrome/browser/cocoa/bookmarks/bookmark_bar_folder_hover_state.h"
#import "chrome/browser/cocoa/bookmarks/bookmark_bar_folder_view.h"
#import "chrome/browser/cocoa/bookmarks/bookmark_folder_target.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#import "chrome/browser/cocoa/event_utils.h"
#import "chrome/browser/themes/browser_theme_provider.h"

namespace {

// Frequency of the scrolling timer in seconds.
const NSTimeInterval kBookmarkBarFolderScrollInterval = 0.1;

// Amount to scroll by per timer fire.  We scroll rather slowly; to
// accomodate we do several at a time.
const CGFloat kBookmarkBarFolderScrollAmount =
    3 * (bookmarks::kBookmarkButtonHeight +
         bookmarks::kBookmarkVerticalPadding);

// Amount to scroll for each scroll wheel delta.
const CGFloat kBookmarkBarFolderScrollWheelAmount =
    1 * (bookmarks::kBookmarkButtonHeight +
         bookmarks::kBookmarkVerticalPadding);

// When constraining a scrolling bookmark bar folder window to the
// screen, shrink the "constrain" by this much vertically.  Currently
// this is 0.0 to avoid a problem with tracking areas leaving the
// window, but should probably be 8.0 or something.
// TODO(jrg): http://crbug.com/36225
const CGFloat kScrollWindowVerticalMargin = 0.0;

}  // namespace

@interface BookmarkBarFolderController(Private)
- (void)configureWindow;
- (void)addOrUpdateScrollTracking;
- (void)removeScrollTracking;
- (void)endScroll;
- (void)addScrollTimerWithDelta:(CGFloat)delta;

// Determine the best button width (which will be the widest button or the
// maximum allowable button width, whichever is less) and resize all buttons.
// Return the new width (so that the window can be adjusted, if necessary).
- (CGFloat)adjustButtonWidths;

// Returns the total menu height needed to display |buttonCount| buttons.
// Does not do any fancy tricks like trimming the height to fit on the screen.
- (int)windowHeightForButtonCount:(int)buttonCount;

// Adjust the height and horizontal position of the window such that the
// scroll arrows are shown as needed and the window appears completely
// on screen.
- (void)adjustWindowForHeight:(int)windowHeight;

// Show or hide the scroll arrows at the top/bottom of the window.
- (void)showOrHideScrollArrows;

// |point| is in the base coordinate system of the destination window;
// it comes from an id<NSDraggingInfo>. |copy| is YES if a copy is to be
// made and inserted into the new location while leaving the bookmark in
// the old location, otherwise move the bookmark by removing from its old
// location and inserting into the new location.
- (BOOL)dragBookmark:(const BookmarkNode*)sourceNode
                  to:(NSPoint)point
                copy:(BOOL)copy;

@end

@interface BookmarkButton (BookmarkBarFolderMenuHighlighting)

// Make the button's border frame always appear when |forceOn| is YES,
// otherwise only border the button when the mouse is inside the button.
- (void)forceButtonBorderToStayOnAlways:(BOOL)forceOn;

// On 10.6 event dispatch for an NSButtonCell's
// showsBorderOnlyWhileMouseInside seems broken if scrolling the
// view that contains the button.  It appears that a mouseExited:
// gets lost, so the button stays highlit forever.  We accomodate
// here.
- (void)toggleButtonBorderingWhileMouseInside;
@end

@implementation BookmarkButton (BookmarkBarFolderMenuHighlighting)

- (void)forceButtonBorderToStayOnAlways:(BOOL)forceOn {
  [self setShowsBorderOnlyWhileMouseInside:!forceOn];
  [self setNeedsDisplay];
}

- (void)toggleButtonBorderingWhileMouseInside {
  BOOL toggle = [self showsBorderOnlyWhileMouseInside];
  [self setShowsBorderOnlyWhileMouseInside:!toggle];
  [self setShowsBorderOnlyWhileMouseInside:toggle];
}

@end

@implementation BookmarkBarFolderController

@synthesize subFolderGrowthToRight = subFolderGrowthToRight_;

- (id)initWithParentButton:(BookmarkButton*)button
          parentController:(BookmarkBarFolderController*)parentController
             barController:(BookmarkBarController*)barController {
  NSString* nibPath =
      [mac_util::MainAppBundle() pathForResource:@"BookmarkBarFolderWindow"
                                          ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibPath owner:self])) {
    parentButton_.reset([button retain]);

    // We want the button to remain bordered as part of the menu path.
    [button forceButtonBorderToStayOnAlways:YES];

    parentController_.reset([parentController retain]);
    if (!parentController_)
      [self setSubFolderGrowthToRight:YES];
    else
      [self setSubFolderGrowthToRight:[parentController
                                        subFolderGrowthToRight]];
    barController_ = barController;  // WEAK
    buttons_.reset([[NSMutableArray alloc] init]);
    folderTarget_.reset([[BookmarkFolderTarget alloc] initWithController:self]);
    NSImage* image = nsimage_cache::ImageNamed(@"menu_overflow_up.pdf");
    DCHECK(image);
    verticalScrollArrowHeight_ = [image size].height;
    [self configureWindow];
    hoverState_.reset([[BookmarkBarFolderHoverState alloc] init]);
  }
  return self;
}

- (void)dealloc {
  // The button is no longer part of the menu path.
  [parentButton_ forceButtonBorderToStayOnAlways:NO];
  [parentButton_ setNeedsDisplay];

  [self removeScrollTracking];
  [self endScroll];
  [hoverState_ draggingExited];

  // Delegate pattern does not retain; make sure pointers to us are removed.
  for (BookmarkButton* button in buttons_.get()) {
    [button setDelegate:nil];
    [button setTarget:nil];
    [button setAction:nil];
  }

  // Note: we don't need to
  //   [NSObject cancelPreviousPerformRequestsWithTarget:self];
  // Because all of our performSelector: calls use withDelay: which
  // retains us.
  [super dealloc];
}

// Overriden from NSWindowController to call childFolderWillShow: before showing
// the window.
- (void)showWindow:(id)sender {
  [barController_ childFolderWillShow:self];
  [super showWindow:sender];
}

- (BookmarkButton*)parentButton {
  return parentButton_.get();
}

- (void)offsetFolderMenuWindow:(NSSize)offset {
  NSWindow* window = [self window];
  NSRect windowFrame = [window frame];
  windowFrame.origin.x -= offset.width;
  windowFrame.origin.y += offset.height;  // Yes, in the opposite direction!
  [window setFrame:windowFrame display:YES];
  [folderController_ offsetFolderMenuWindow:offset];
}

- (void)reconfigureMenu {
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  for (BookmarkButton* button in buttons_.get()) {
    [button setDelegate:nil];
    [button removeFromSuperview];
  }
  [buttons_ removeAllObjects];
  [self configureWindow];
}

#pragma mark Private Methods

- (BookmarkButtonCell*)cellForBookmarkNode:(const BookmarkNode*)child {
  NSImage* image = child ? [barController_ favIconForNode:child] : nil;
  NSMenu* menu = child ? child->is_folder() ? folderMenu_ : buttonMenu_ : nil;
  BookmarkBarFolderButtonCell* cell =
      [BookmarkBarFolderButtonCell buttonCellForNode:child
                                         contextMenu:menu
                                            cellText:nil
                                           cellImage:image];
  [cell setTag:kStandardButtonTypeWithLimitedClickFeedback];
  return cell;
}

// Redirect to our logic shared with BookmarkBarController.
- (IBAction)openBookmarkFolderFromButton:(id)sender {
  [folderTarget_ openBookmarkFolderFromButton:sender];
}

// Create a bookmark button for the given node using frame.
//
// If |node| is NULL this is an "(empty)" button.
// Does NOT add this button to our button list.
// Returns an autoreleased button.
// Adjusts the input frame width as appropriate.
//
// TODO(jrg): combine with addNodesToButtonList: code from
// bookmark_bar_controller.mm, and generalize that to use both x and y
// offsets.
// http://crbug.com/35966
- (BookmarkButton*)makeButtonForNode:(const BookmarkNode*)node
                               frame:(NSRect)frame {
  BookmarkButtonCell* cell = [self cellForBookmarkNode:node];
  DCHECK(cell);

  // We must decide if we draw the folder arrow before we ask the cell
  // how big it needs to be.
  if (node && node->is_folder()) {
    // Warning when combining code with bookmark_bar_controller.mm:
    // this call should NOT be made for the bar buttons; only for the
    // subfolder buttons.
    [cell setDrawFolderArrow:YES];
  }

  // The "+2" is needed because, sometimes, Cocoa is off by a tad when
  // returning the value it thinks it needs.
  CGFloat desired = [cell cellSize].width + 2;
  // The width is determined from the maximum of the proposed width
  // (provided in |frame|) or the natural width of the title, then
  // limited by the abolute minimum and maximum allowable widths.
  frame.size.width =
      std::min(std::max(bookmarks::kBookmarkMenuButtonMinimumWidth,
                        std::max(frame.size.width, desired)),
               bookmarks::kBookmarkMenuButtonMaximumWidth);

  BookmarkButton* button = [[[BookmarkButton alloc] initWithFrame:frame]
                               autorelease];
  DCHECK(button);

  [button setCell:cell];
  [button setDelegate:self];
  if (node) {
    if (node->is_folder()) {
      [button setTarget:self];
      [button setAction:@selector(openBookmarkFolderFromButton:)];
    } else {
      // Make the button do something.
      [button setTarget:self];
      [button setAction:@selector(openBookmark:)];
      // Add a tooltip.
      NSString* title = base::SysUTF16ToNSString(node->GetTitle());
      std::string urlString = node->GetURL().possibly_invalid_spec();
      NSString* tooltip = [NSString stringWithFormat:@"%@\n%s", title,
                                    urlString.c_str()];
      [button setToolTip:tooltip];
    }
  } else {
    [button setEnabled:NO];
    [button setBordered:NO];
  }
  return button;
}

// Exposed for testing.
- (NSView*)mainView {
  return mainView_;
}

- (id)folderTarget {
  return folderTarget_.get();
}


// Our parent controller is another BookmarkBarFolderController, so
// our window is to the right or left of it.  We use a little overlap
// since it looks much more menu-like than with none.  If we would
// grow off the screen, switch growth to the other direction.  Growth
// direction sticks for folder windows which are descendents of us.
// If we have tried both directions and neither fits, degrade to a
// default.
- (CGFloat)childFolderWindowLeftForWidth:(int)windowWidth {
  // We may legitimately need to try two times (growth to right and
  // left but not in that order).  Limit us to three tries in case
  // the folder window can't fit on either side of the screen; we
  // don't want to loop forever.
  CGFloat x;
  int tries = 0;
  while (tries < 2) {
    // Try to grow right.
    if ([self subFolderGrowthToRight]) {
      tries++;
      x = NSMaxX([[parentButton_ window] frame]) -
          bookmarks::kBookmarkMenuOverlap;
      // If off the screen, switch direction.
      if ((x + windowWidth +
           bookmarks::kBookmarkHorizontalScreenPadding) >
          NSMaxX([[[self window] screen] frame])) {
        [self setSubFolderGrowthToRight:NO];
      } else {
        return x;
      }
    }
    // Try to grow left.
    if (![self subFolderGrowthToRight]) {
      tries++;
      x = NSMinX([[parentButton_ window] frame]) +
          bookmarks::kBookmarkMenuOverlap -
          windowWidth;
      // If off the screen, switch direction.
      if (x < NSMinX([[[self window] screen] frame])) {
        [self setSubFolderGrowthToRight:YES];
      } else {
        return x;
      }
    }
  }
  // Unhappy; do the best we can.
  return NSMaxX([[[self window] screen] frame]) - windowWidth;
}


// Compute and return the top left point of our window (screen
// coordinates).  The top left is positioned in a manner similar to
// cascading menus.  Windows may grow to either the right or left of
// their parent (if a sub-folder) so we need to know |windowWidth|.
- (NSPoint)windowTopLeftForWidth:(int)windowWidth {
  NSPoint newWindowTopLeft;
  if (![parentController_ isKindOfClass:[self class]]) {
    // If we're not popping up from one of ourselves, we must be
    // popping up from the bookmark bar itself.  In this case, start
    // BELOW the parent button.  Our left is the button left; our top
    // is bottom of button's parent view.
    NSPoint buttonBottomLeftInScreen =
        [[parentButton_ window]
            convertBaseToScreen:[parentButton_
                                    convertPoint:NSZeroPoint toView:nil]];
    NSPoint bookmarkBarBottomLeftInScreen =
        [[parentButton_ window]
            convertBaseToScreen:[[parentButton_ superview]
                                    convertPoint:NSZeroPoint toView:nil]];
    newWindowTopLeft = NSMakePoint(buttonBottomLeftInScreen.x,
                                   bookmarkBarBottomLeftInScreen.y);
    // Make sure the window is on-screen; if not, push left.  It is
    // intentional that top level folders "push left" slightly
    // different than subfolders.
    NSRect screenFrame = [[[parentButton_ window] screen] frame];
    CGFloat spillOff = (newWindowTopLeft.x + windowWidth) - NSMaxX(screenFrame);
    if (spillOff > 0.0) {
      newWindowTopLeft.x = std::max(newWindowTopLeft.x - spillOff,
                                    NSMinX(screenFrame));
    }
  } else {
    // Parent is a folder; grow right/left.
    newWindowTopLeft.x = [self childFolderWindowLeftForWidth:windowWidth];
    NSPoint top = NSMakePoint(0, (NSMaxY([parentButton_ frame]) +
                                  bookmarks::kBookmarkVerticalPadding));
    NSPoint topOfWindow =
        [[parentButton_ window]
            convertBaseToScreen:[[parentButton_ superview]
                                    convertPoint:top toView:nil]];
    newWindowTopLeft.y = topOfWindow.y;
  }
  return newWindowTopLeft;
}

// Set our window level to the right spot so we're above the menubar, dock, etc.
// Factored out so we can override/noop in a unit test.
- (void)configureWindowLevel {
  [[self window] setLevel:NSPopUpMenuWindowLevel];
}

- (int)windowHeightForButtonCount:(int)buttonCount {
  return (buttonCount * (bookmarks::kBookmarkButtonHeight +
                         bookmarks::kBookmarkVerticalPadding)) +
      bookmarks::kBookmarkVerticalPadding;
}

- (void)adjustWindowForHeight:(int)windowHeight {
  // Adjust all button widths to be consistent, determine the best size for
  // the window, and set the window frame.
  CGFloat windowWidth =
      [self adjustButtonWidths] +
      (2 * bookmarks::kBookmarkSubMenuHorizontalPadding);
  NSPoint newWindowTopLeft = [self windowTopLeftForWidth:windowWidth];
  NSSize windowSize = NSMakeSize(windowWidth, windowHeight);
  windowSize = [scrollView_ convertSize:windowSize toView:nil];
  NSWindow* window = [self window];
  // If the window is already visible then make sure its top remains stable.
  BOOL windowAlreadyShowing = [window isVisible];
  CGFloat deltaY = windowHeight - NSHeight([mainView_ frame]);
  if (windowAlreadyShowing) {
    NSRect oldFrame = [window frame];
    newWindowTopLeft.y = oldFrame.origin.y + NSHeight(oldFrame);
  }
  NSRect windowFrame = NSMakeRect(newWindowTopLeft.x,
      newWindowTopLeft.y - windowHeight, windowSize.width, windowHeight);
  // Make the scrolled content be the right size (full size).
  NSRect mainViewFrame = NSMakeRect(0, 0, NSWidth(windowFrame) -
      bookmarks::kScrollViewContentWidthMargin, NSHeight(windowFrame));
  [mainView_ setFrame:mainViewFrame];
  // Make sure the window fits on the screen.  If not, constrain.
  // We'll scroll to allow the user to see all the content.
  NSRect screenFrame = [[[self window] screen] frame];
  screenFrame = NSInsetRect(screenFrame, 0, kScrollWindowVerticalMargin);
  BOOL wasScrollable = scrollable_;
  if (!NSContainsRect(screenFrame, windowFrame)) {
    scrollable_ = YES;
    windowFrame = NSIntersectionRect(screenFrame, windowFrame);
  } else {
    scrollable_ = NO;
  }
  [window setFrame:windowFrame display:NO];
  if (wasScrollable != scrollable_) {
    // If scrollability changed then rework visibility of the scroll arrows
    // and the scroll offset of the menu view.
    NSSize windowLocalSize =
        [scrollView_ convertSize:windowFrame.size fromView:nil];
    CGFloat scrollPointY = NSHeight(mainViewFrame) - windowLocalSize.height +
        bookmarks::kBookmarkVerticalPadding;
    [mainView_ scrollPoint:NSMakePoint(0, scrollPointY)];
    [self showOrHideScrollArrows];
    [self addOrUpdateScrollTracking];
  } else if (scrollable_ && windowAlreadyShowing) {
    // If the window was already showing and is still scrollable then make
    // sure the main view moves upward, not downward so that the content
    // at the bottom of the menu, not the top, appears to move.
    // The edge case is when the menu is scrolled all the way to top (hence
    // the test of scrollDownArrowShown_) - don't scroll then.
    NSView* superView = [mainView_ superview];
    DCHECK([superView isKindOfClass:[NSClipView class]]);
    NSClipView* clipView = static_cast<NSClipView*>(superView);
    CGFloat scrollPointY = [clipView bounds].origin.y +
        bookmarks::kBookmarkVerticalPadding;
    if (scrollDownArrowShown_ || deltaY > 0.0)
      scrollPointY += deltaY;
    [mainView_ scrollPoint:NSMakePoint(0, scrollPointY)];
  }
  [window display];
}

// Determine window size and position.
// Create buttons for all our nodes.
// TODO(jrg): break up into more and smaller routines for easier unit testing.
- (void)configureWindow {
  const BookmarkNode* node = [parentButton_ bookmarkNode];
  DCHECK(node);
  int startingIndex = [[parentButton_ cell] startingChildIndex];
  DCHECK_LE(startingIndex, node->GetChildCount());
  // Must have at least 1 button (for "empty")
  int buttons = std::max(node->GetChildCount() - startingIndex, 1);

  // Prelim height of the window.  We'll trim later as needed.
  int height = [self windowHeightForButtonCount:buttons];
  // We'll need this soon...
  [self window];

  // TODO(jrg): combine with frame code in bookmark_bar_controller.mm
  // http://crbug.com/35966
  NSRect buttonsOuterFrame = NSMakeRect(
    bookmarks::kBookmarkSubMenuHorizontalPadding,
    (height -
     (bookmarks::kBookmarkButtonHeight + bookmarks::kBookmarkVerticalPadding)),
    bookmarks::kDefaultBookmarkWidth,
    bookmarks::kBookmarkButtonHeight);

  // TODO(jrg): combine with addNodesToButtonList: code from
  // bookmark_bar_controller.mm (but use y offset)
  // http://crbug.com/35966
  if (!node->GetChildCount()) {
    // If no children we are the empty button.
    BookmarkButton* button = [self makeButtonForNode:nil
                                               frame:buttonsOuterFrame];
    [buttons_ addObject:button];
    [mainView_ addSubview:button];
  } else {
    for (int i = startingIndex;
         i < node->GetChildCount();
         i++) {
      const BookmarkNode* child = node->GetChild(i);
      BookmarkButton* button = [self makeButtonForNode:child
                                                 frame:buttonsOuterFrame];
      [buttons_ addObject:button];
      [mainView_ addSubview:button];
      buttonsOuterFrame.origin.y -= (bookmarks::kBookmarkButtonHeight +
                                     bookmarks::kBookmarkVerticalPadding);
    }
  }

  [self adjustWindowForHeight:height];
  // Finally pop me up.
  [self configureWindowLevel];
}

// TODO(mrossetti): See if the following can be moved into view's viewWillDraw:.
- (CGFloat)adjustButtonWidths {
  CGFloat width = bookmarks::kBookmarkMenuButtonMinimumWidth;
  // Use the cell's size as the base for determining the desired width of the
  // button rather than the button's current width. -[cell cellSize] always
  // returns the 'optimum' size of the cell based on the cell's contents even
  // if it's less than the current button size. Relying on the button size
  // would result in buttons that could only get wider but we want to handle
  // the case where the widest button gets removed from a folder menu.
  for (BookmarkButton* button in buttons_.get())
    width = std::max(width, [[button cell] cellSize].width);
  width = std::min(width, bookmarks::kBookmarkMenuButtonMaximumWidth);
  // Things look and feel more menu-like if all the buttons are the
  // full width of the window, especially if there are submenus.
  for (BookmarkButton* button in buttons_.get()) {
    NSRect buttonFrame = [button frame];
    buttonFrame.size.width = width;
    [button setFrame:buttonFrame];
  }
  return width;
}

- (BOOL)canScrollUp {
  // If removal of an arrow would make things "finished", state as
  // such.
  CGFloat scrollY = [scrollView_ documentVisibleRect].origin.y;
  if (scrollUpArrowShown_)
    scrollY -= verticalScrollArrowHeight_;

  if (scrollY <= 0)
    return NO;
  return YES;
}

- (BOOL)canScrollDown {
  CGFloat arrowAdjustment = 0.0;

  // We do NOT adjust based on the scrollDOWN arrow.  This keeps
  // things from "jumping"; if removal of the down arrow (at the top
  // of the window) would cause a scroll to end, we'll end.
  if (scrollUpArrowShown_)
    arrowAdjustment += verticalScrollArrowHeight_;

  NSPoint scrollPosition = [scrollView_ documentVisibleRect].origin;
  NSRect documentRect = [[scrollView_ documentView] frame];

  // If we are exactly the right height, return no.  We need this
  // extra conditional in the case where we've just scrolled/grown
  // into position.
  if (NSHeight([[self window] frame]) == NSHeight(documentRect))
    return NO;

  if ((scrollPosition.y + NSHeight([[self window] frame])) >=
      (NSHeight(documentRect) + arrowAdjustment)) {
    return NO;
  }
  return YES;
}

- (void)showOrHideScrollArrows {
  NSRect frame = [scrollView_ frame];
  CGFloat scrollDelta = 0.0;
  BOOL canScrollDown = [self canScrollDown];
  BOOL canScrollUp = [self canScrollUp];

  if (canScrollUp != scrollUpArrowShown_) {
    if (scrollUpArrowShown_) {
      frame.origin.y -= verticalScrollArrowHeight_;
      frame.size.height += verticalScrollArrowHeight_;
      scrollDelta = verticalScrollArrowHeight_;
    } else {
      frame.origin.y += verticalScrollArrowHeight_;
      frame.size.height -= verticalScrollArrowHeight_;
      scrollDelta = -verticalScrollArrowHeight_;
    }
  }
  if (canScrollDown != scrollDownArrowShown_) {
    if (scrollDownArrowShown_) {
      frame.size.height += verticalScrollArrowHeight_;
    } else {
      frame.size.height -= verticalScrollArrowHeight_;
    }
  }
  scrollUpArrowShown_ = canScrollUp;
  scrollDownArrowShown_ = canScrollDown;
  [scrollView_ setFrame:frame];

  // Adjust scroll based on new frame.  For example, if we make room
  // for an arrow at the bottom, adjust the scroll so the topmost item
  // is still fully visible.
  if (scrollDelta) {
    NSPoint scrollPosition = [scrollView_ documentVisibleRect].origin;
    scrollPosition.y -= scrollDelta;
    [[scrollView_ documentView] scrollPoint:scrollPosition];
  }
}

- (BOOL)scrollable {
  return scrollable_;
}

// Start a "scroll up" timer.
- (void)beginScrollWindowUp {
  [self addScrollTimerWithDelta:kBookmarkBarFolderScrollAmount];
}

// Start a "scroll down" timer.
- (void)beginScrollWindowDown {
  [self addScrollTimerWithDelta:-kBookmarkBarFolderScrollAmount];
}

// End a scrolling timer.  Can be called excessively with no harm.
- (void)endScroll {
  if (scrollTimer_) {
    [scrollTimer_ invalidate];
    scrollTimer_ = nil;
    verticalScrollDelta_ = 0;
  }
}

// Perform a single scroll of the specified amount.
// Scroll up:
// Scroll the documentView by the growth amount.
// If we cannot grow the window, simply scroll the documentView.
// If we can grow the window up without falling off the screen, do it.
// Scroll down:
// Never change the window size; only scroll the documentView.
- (void)performOneScroll:(CGFloat)delta {
  NSRect windowFrame = [[self window] frame];
  NSRect screenFrame = [[[self window] screen] frame];

  // First scroll the "document" area.
  NSPoint scrollPosition = [scrollView_ documentVisibleRect].origin;
  scrollPosition.y -= delta;
  [[scrollView_ documentView] scrollPoint:scrollPosition];

  if (buttonThatMouseIsIn_)
    [buttonThatMouseIsIn_ toggleButtonBorderingWhileMouseInside];

  // We update the window size after shifting the scroll to avoid a race.
  CGFloat screenHeightMinusMargin = (NSHeight(screenFrame) -
                                     (2 * kScrollWindowVerticalMargin));
  if (delta) {
    // If we can, grow the window (up).
    if (NSHeight(windowFrame) < screenHeightMinusMargin) {
      CGFloat growAmount = delta;
      // Don't scroll more than enough to "finish".
      if (scrollPosition.y < 0)
        growAmount += scrollPosition.y;
      windowFrame.size.height += growAmount;
      windowFrame.size.height = std::min(NSHeight(windowFrame),
                                         screenHeightMinusMargin);
      // Watch out for a finish that isn't the full height of the screen.
      // We get here if using the scroll wheel to scroll by small amounts.
      windowFrame.size.height = std::min(NSHeight(windowFrame),
                                         NSHeight([mainView_ frame]));
      // Don't allow scrolling to make the window smaller, ever.  This
      // conditional is important when processing scrollWheel events.
      if (windowFrame.size.height > [[self window] frame].size.height) {
        [[self window] setFrame:windowFrame display:YES];
        [self addOrUpdateScrollTracking];
      }
    }
  }

  // If we're at either end, happiness.
  if ((scrollPosition.y <= 0) ||
      ((scrollPosition.y + NSHeight(windowFrame) >=
        NSHeight([mainView_ frame])) &&
       (windowFrame.size.height == screenHeightMinusMargin))) {
    [self endScroll];

    // If we can't scroll either up or down we are completely done.
    // For example, perhaps we've scrolled a little and grown the
    // window on-screen until there is now room for everything.
    if (![self canScrollUp] && ![self canScrollDown]) {
      scrollable_ = NO;
      [self removeScrollTracking];
    }
  }

  [self showOrHideScrollArrows];
}

// Perform a scroll of the window on the screen.
// Called by a timer when scrolling.
- (void)performScroll:(NSTimer*)timer {
  DCHECK(verticalScrollDelta_);
  [self performOneScroll:verticalScrollDelta_];
}


// Add a timer to fire at a regular interveral which scrolls the
// window vertically |delta|.
- (void)addScrollTimerWithDelta:(CGFloat)delta {
  if (scrollTimer_ && verticalScrollDelta_ == delta)
    return;
  [self endScroll];
  verticalScrollDelta_ = delta;
  scrollTimer_ =
      [NSTimer scheduledTimerWithTimeInterval:kBookmarkBarFolderScrollInterval
                                       target:self
                                     selector:@selector(performScroll:)
                                     userInfo:nil
                                      repeats:YES];
}

// Called as a result of our tracking area.  Warning: on the main
// screen (of a single-screened machine), the minimum mouse y value is
// 1, not 0.  Also, we do not get events when the mouse is above the
// menubar (to be fixed by setting the proper window level; see
// initializer).
- (void)mouseMoved:(NSEvent*)theEvent {
  DCHECK([theEvent window] == [self window]);

  NSPoint eventScreenLocation =
      [[theEvent window] convertBaseToScreen:[theEvent locationInWindow]];

  // We use frame (not visibleFrame) since our bookmark folder is on
  // TOP of the menubar.
  NSRect visibleRect = [[[self window] screen] frame];
  CGFloat closeToTopOfScreen = NSMaxY(visibleRect) -
      verticalScrollArrowHeight_;
  CGFloat closeToBottomOfScreen = NSMinY(visibleRect) +
      verticalScrollArrowHeight_;

  if (eventScreenLocation.y <= closeToBottomOfScreen) {
    [self beginScrollWindowUp];
  } else if (eventScreenLocation.y > closeToTopOfScreen) {
    [self beginScrollWindowDown];
  } else {
    [self endScroll];
  }
}

- (void)mouseExited:(NSEvent*)theEvent {
  [self endScroll];
}

// Add a tracking area so we know when the mouse is pinned to the top
// or bottom of the screen.  If that happens, and if the mouse
// position overlaps the window, scroll it.
- (void)addOrUpdateScrollTracking {
  [self removeScrollTracking];
  NSView* view = [[self window] contentView];
  scrollTrackingArea_.reset([[NSTrackingArea alloc]
                              initWithRect:[view bounds]
                                   options:(NSTrackingMouseMoved |
                                            NSTrackingMouseEnteredAndExited |
                                            NSTrackingActiveAlways)
                                     owner:self
                                  userInfo:nil]);
    [view addTrackingArea:scrollTrackingArea_];
}

// Remove the tracking area associated with scrolling.
- (void)removeScrollTracking {
  if (scrollTrackingArea_.get()) {
    [[[self window] contentView] removeTrackingArea:scrollTrackingArea_];
  }
  scrollTrackingArea_.reset();
}

// Delegate callback.
- (void)windowWillClose:(NSNotification*)notification {
  // If a "hover open" is pending when the bookmark bar folder is
  // closed, be sure it gets cancelled.
  [NSObject cancelPreviousPerformRequestsWithTarget:self];

  [barController_ childFolderWillClose:self];
  [self closeBookmarkFolder:self];
  [self autorelease];
}

// Close the old hover-open bookmark folder, and open a new one.  We
// do both in one step to allow for a delay in closing the old one.
// See comments above kDragHoverCloseDelay (bookmark_bar_controller.h)
// for more details.
- (void)openBookmarkFolderFromButtonAndCloseOldOne:(id)sender {
  // If an old submenu exists, close it immediately.
  [self closeBookmarkFolder:sender];

  // Open a new one if meaningful.
  if ([sender isFolder])
    [folderTarget_ openBookmarkFolderFromButton:sender];
}

- (NSArray*)buttons {
  return buttons_.get();
}

- (void)close {
  [folderController_ close];
  [super close];
}

- (void)scrollWheel:(NSEvent *)theEvent {
  if (scrollable_) {
    // We go negative since an NSScrollView has a flipped coordinate frame.
    CGFloat amt = kBookmarkBarFolderScrollWheelAmount * -[theEvent deltaY];
    [self performOneScroll:amt];
  }
}

#pragma mark Actions Forwarded to Parent BookmarkBarController

- (IBAction)openBookmark:(id)sender {
  [barController_ openBookmark:sender];
}

- (IBAction)openBookmarkInNewForegroundTab:(id)sender {
  [barController_ openBookmarkInNewForegroundTab:sender];
}

- (IBAction)openBookmarkInNewWindow:(id)sender {
  [barController_ openBookmarkInNewWindow:sender];
}

- (IBAction)openBookmarkInIncognitoWindow:(id)sender {
  [barController_ openBookmarkInIncognitoWindow:sender];
}

- (IBAction)editBookmark:(id)sender {
  [barController_ editBookmark:sender];
}

- (IBAction)cutBookmark:(id)sender {
  [self closeBookmarkFolder:self];
  [barController_ cutBookmark:sender];
}

- (IBAction)copyBookmark:(id)sender {
  [barController_ copyBookmark:sender];
}

- (IBAction)pasteBookmark:(id)sender {
  [barController_ pasteBookmark:sender];
}

- (IBAction)deleteBookmark:(id)sender {
  [self closeBookmarkFolder:self];
  [barController_ deleteBookmark:sender];
}

- (IBAction)openAllBookmarks:(id)sender {
  [barController_ openAllBookmarks:sender];
}

- (IBAction)openAllBookmarksNewWindow:(id)sender {
  [barController_ openAllBookmarksNewWindow:sender];
}

- (IBAction)openAllBookmarksIncognitoWindow:(id)sender {
  [barController_ openAllBookmarksIncognitoWindow:sender];
}

- (IBAction)addPage:(id)sender {
  [barController_ addPage:sender];
}

- (IBAction)addFolder:(id)sender {
  [barController_ addFolder:sender];
}

#pragma mark Drag & Drop

// Find something like std::is_between<T>?  I can't believe one doesn't exist.
// http://crbug.com/35966
static BOOL ValueInRangeInclusive(CGFloat low, CGFloat value, CGFloat high) {
  return ((value >= low) && (value <= high));
}

// Return the proposed drop target for a hover open button, or nil if none.
//
// TODO(jrg): this is just like the version in
// bookmark_bar_controller.mm, but vertical instead of horizontal.
// Generalize to be axis independent then share code.
// http://crbug.com/35966
// Get UI review on "middle half" ness.
// http://crbug.com/36276
- (BookmarkButton*)buttonForDroppingOnAtPoint:(NSPoint)point {
  for (BookmarkButton* button in buttons_.get()) {
    // No early break -- makes no assumption about button ordering.

    // Intentionally NOT using NSPointInRect() so that scrolling into
    // a submenu doesn't cause it to be closed.
    if (ValueInRangeInclusive(NSMinY([button frame]),
                              point.y,
                              NSMaxY([button frame]))) {

      // Over a button but let's be a little more specific
      // (e.g. over the middle half).
      NSRect frame = [button frame];
      NSRect middleHalfOfButton = NSInsetRect(frame, 0, frame.size.height / 4);
      if (ValueInRangeInclusive(NSMinY(middleHalfOfButton),
                                point.y,
                                NSMaxY(middleHalfOfButton))) {
        // It makes no sense to drop on a non-folder; there is no hover.
        if (![button isFolder])
          return nil;
        // Got it!
        return button;
      } else {
        // Over a button but not over the middle half.
        return nil;
      }
    }
  }
  // Not hovering over a button.
  return nil;
}

// TODO(jrg): again we have code dup, sort of, with
// bookmark_bar_controller.mm, but the axis is changed.  One minor
// difference is accomodation for the "empty" button (which may not
// exist in the future).
// http://crbug.com/35966
- (int)indexForDragToPoint:(NSPoint)point {
  // Identify which buttons we are between.  For now, assume a button
  // location is at the center point of its view, and that an exact
  // match means "place before".
  // TODO(jrg): revisit position info based on UI team feedback.
  // dropLocation is in bar local coordinates.
  // http://crbug.com/36276
  NSPoint dropLocation =
      [mainView_ convertPoint:point
                     fromView:[[self window] contentView]];
  BookmarkButton* buttonToTheTopOfDraggedButton = nil;
  // Buttons are laid out in this array from top to bottom (screen
  // wise), which means "biggest y" --> "smallest y".
  for (BookmarkButton* button in buttons_.get()) {
    CGFloat midpoint = NSMidY([button frame]);
    if (dropLocation.y > midpoint) {
      break;
    }
    buttonToTheTopOfDraggedButton = button;
  }

  // TODO(jrg): On Windows, dropping onto (empty) highlights the
  // entire drop location and does not use an insertion point.
  // http://crbug.com/35967
  if (!buttonToTheTopOfDraggedButton) {
    // We are at the very top (we broke out of the loop on the first try).
    return 0;
  }
  if ([buttonToTheTopOfDraggedButton isEmpty]) {
    // There is a button but it's an empty placeholder.
    // Default to inserting on top of it.
    return 0;
  }
  const BookmarkNode* beforeNode = [buttonToTheTopOfDraggedButton
                                       bookmarkNode];
  DCHECK(beforeNode);
  // Be careful if the number of buttons != number of nodes.
  return ((beforeNode->GetParent()->IndexOfChild(beforeNode) + 1) -
          [[parentButton_ cell] startingChildIndex]);
}

// TODO(jrg): Yet more code dup.
// http://crbug.com/35966
- (BOOL)dragBookmark:(const BookmarkNode*)sourceNode
                  to:(NSPoint)point
                copy:(BOOL)copy {
  DCHECK(sourceNode);

  // Drop destination.
  const BookmarkNode* destParent = NULL;
  int destIndex = 0;

  // First check if we're dropping on a button.  If we have one, and
  // it's a folder, drop in it.
  BookmarkButton* button = [self buttonForDroppingOnAtPoint:point];
  if ([button isFolder]) {
    destParent = [button bookmarkNode];
    // Drop it at the end.
    destIndex = [button bookmarkNode]->GetChildCount();
  } else {
    // Else we're dropping somewhere in the folder, so find the right spot.
    destParent = [parentButton_ bookmarkNode];
    destIndex = [self indexForDragToPoint:point];
    // Be careful if the number of buttons != number of nodes.
    destIndex += [[parentButton_ cell] startingChildIndex];
  }

  // Prevent cycles.
  BOOL wasCopiedOrMoved = NO;
  if (!destParent->HasAncestor(sourceNode)) {
    if (copy)
      [self bookmarkModel]->Copy(sourceNode, destParent, destIndex);
    else
      [self bookmarkModel]->Move(sourceNode, destParent, destIndex);
    wasCopiedOrMoved = YES;
    // Movement of a node triggers observers (like us) to rebuild the
    // bar so we don't have to do so explicitly.
  }

  return wasCopiedOrMoved;
}

#pragma mark BookmarkButtonDelegate Protocol

- (void)fillPasteboard:(NSPasteboard*)pboard
       forDragOfButton:(BookmarkButton*)button {
  [[self folderTarget] fillPasteboard:pboard forDragOfButton:button];

  // Close our folder menu and submenus since we know we're going to be dragged.
  [self closeBookmarkFolder:self];
}

// Called from BookmarkButton.
// Unlike bookmark_bar_controller's version, we DO default to being enabled.
- (void)mouseEnteredButton:(id)sender event:(NSEvent*)event {
  buttonThatMouseIsIn_ = sender;

  // Cancel a previous hover if needed.
  [NSObject cancelPreviousPerformRequestsWithTarget:self];

  // If already opened, then we exited but re-entered the button
  // (without entering another button open), do nothing.
  if ([folderController_ parentButton] == sender)
    return;

  [self performSelector:@selector(openBookmarkFolderFromButtonAndCloseOldOne:)
             withObject:sender
             afterDelay:bookmarks::kHoverOpenDelay];
}

// Called from the BookmarkButton
- (void)mouseExitedButton:(id)sender event:(NSEvent*)event {
  if (buttonThatMouseIsIn_ == sender)
    buttonThatMouseIsIn_ = nil;

  // Stop any timer about opening a new hover-open folder.

  // Since a performSelector:withDelay: on self retains self, it is
  // possible that a cancelPreviousPerformRequestsWithTarget: reduces
  // the refcount to 0, releasing us.  That's a bad thing to do while
  // this object (or others it may own) is in the event chain.  Thus
  // we have a retain/autorelease.
  [self retain];
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  [self autorelease];
}

- (NSWindow*)browserWindow {
  return [parentController_ browserWindow];
}

- (BOOL)canDragBookmarkButtonToTrash:(BookmarkButton*)button {
  return [barController_ canEditBookmark:[button bookmarkNode]];
}

- (void)didDragBookmarkToTrash:(BookmarkButton*)button {
  // TODO(mrossetti): Refactor BookmarkBarFolder common code.
  // http://crbug.com/35966
  const BookmarkNode* node = [button bookmarkNode];
  if (node) {
    const BookmarkNode* parent = node->GetParent();
    [self bookmarkModel]->Remove(parent,
                                 parent->IndexOfChild(node));
  }
}

#pragma mark BookmarkButtonControllerProtocol

// Recursively close all bookmark folders.
- (void)closeAllBookmarkFolders {
  // Closing the top level implicitly closes all children.
  [barController_ closeAllBookmarkFolders];
}

// Close our bookmark folder (a sub-controller) if we have one.
- (void)closeBookmarkFolder:(id)sender {
  if (folderController_) {
    [self setSubFolderGrowthToRight:YES];
    [[folderController_ window] close];
    folderController_ = nil;
  }
}

- (BookmarkModel*)bookmarkModel {
  return [barController_ bookmarkModel];
}

// TODO(jrg): Refactor BookmarkBarFolder common code. http://crbug.com/35966
// Most of the work (e.g. drop indicator) is taken care of in the
// folder_view.  Here we handle hover open issues for subfolders.
// Caution: there are subtle differences between this one and
// bookmark_bar_controller.mm's version.
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info {
  NSPoint currentLocation = [info draggingLocation];
  BookmarkButton* button = [self buttonForDroppingOnAtPoint:currentLocation];

  // Don't allow drops that would result in cycles.
  if (button) {
    NSData* data = [[info draggingPasteboard]
                    dataForType:kBookmarkButtonDragType];
    if (data && [info draggingSource]) {
      BookmarkButton* sourceButton = nil;
      [data getBytes:&sourceButton length:sizeof(sourceButton)];
      const BookmarkNode* sourceNode = [sourceButton bookmarkNode];
      const BookmarkNode* destNode = [button bookmarkNode];
      if (destNode->HasAncestor(sourceNode))
        button = nil;
    }
  }
  // Delegate handling of dragging over a button to the |hoverState_| member.
  return [hoverState_ draggingEnteredButton:button];
}

// Unlike bookmark_bar_controller, we need to keep track of dragging state.
// We also need to make sure we cancel the delayed hover close.
- (void)draggingExited:(id<NSDraggingInfo>)info {
  // NOT the same as a cancel --> we may have moved the mouse into the submenu.
  // Delegate handling of the hover button to the |hoverState_| member.
  [hoverState_ draggingExited];
}

- (BOOL)dragShouldLockBarVisibility {
  return [parentController_ dragShouldLockBarVisibility];
}

// TODO(jrg): ARGH more code dup.
// http://crbug.com/35966
- (BOOL)dragButton:(BookmarkButton*)sourceButton
                to:(NSPoint)point
              copy:(BOOL)copy {
  DCHECK([sourceButton isKindOfClass:[BookmarkButton class]]);
  const BookmarkNode* sourceNode = [sourceButton bookmarkNode];
  return [self dragBookmark:sourceNode to:point copy:copy];
}

// TODO(mrossetti,jrg): Identical to the same function in BookmarkBarController.
// http://crbug.com/35966
- (BOOL)dragBookmarkData:(id<NSDraggingInfo>)info {
  BOOL dragged = NO;
  std::vector<const BookmarkNode*> nodes([self retrieveBookmarkDragDataNodes]);
  if (nodes.size()) {
    BOOL copy = !([info draggingSourceOperationMask] & NSDragOperationMove);
    NSPoint dropPoint = [info draggingLocation];
    for (std::vector<const BookmarkNode*>::const_iterator it = nodes.begin();
         it != nodes.end(); ++it) {
      const BookmarkNode* sourceNode = *it;
      dragged = [self dragBookmark:sourceNode to:dropPoint copy:copy];
    }
  }
  return dragged;
}

// TODO(mrossetti,jrg): Identical to the same function in BookmarkBarController.
// http://crbug.com/35966
- (std::vector<const BookmarkNode*>)retrieveBookmarkDragDataNodes {
  std::vector<const BookmarkNode*> dragDataNodes;
  BookmarkDragData dragData;
  if(dragData.ReadFromDragClipboard()) {
    BookmarkModel* bookmarkModel = [self bookmarkModel];
    Profile* profile = bookmarkModel->profile();
    std::vector<const BookmarkNode*> nodes(dragData.GetNodes(profile));
    dragDataNodes.assign(nodes.begin(), nodes.end());
  }
  return dragDataNodes;
}

// Return YES if we should show the drop indicator, else NO.
// TODO(jrg): ARGH code dup!
// http://crbug.com/35966
- (BOOL)shouldShowIndicatorShownForPoint:(NSPoint)point {
  return ![self buttonForDroppingOnAtPoint:point];
}

// Return the y position for a drop indicator.
//
// TODO(jrg): again we have code dup, sort of, with
// bookmark_bar_controller.mm, but the axis is changed.
// http://crbug.com/35966
- (CGFloat)indicatorPosForDragToPoint:(NSPoint)point {
  CGFloat y = 0;
  int destIndex = [self indexForDragToPoint:point];
  int numButtons = static_cast<int>([buttons_ count]);

  // If it's a drop strictly between existing buttons or at the very beginning
  if (destIndex >= 0 && destIndex < numButtons) {
    // ... put the indicator right between the buttons.
    BookmarkButton* button =
        [buttons_ objectAtIndex:static_cast<NSUInteger>(destIndex)];
    DCHECK(button);
    NSRect buttonFrame = [button frame];
    y = NSMaxY(buttonFrame) + 0.5 * bookmarks::kBookmarkVerticalPadding;

    // If it's a drop at the end (past the last button, if there are any) ...
  } else if (destIndex == numButtons) {
    // and if it's past the last button ...
    if (numButtons > 0) {
      // ... find the last button, and put the indicator below it.
      BookmarkButton* button =
          [buttons_ objectAtIndex:static_cast<NSUInteger>(destIndex - 1)];
      DCHECK(button);
      NSRect buttonFrame = [button frame];
      y = buttonFrame.origin.y - 0.5 * bookmarks::kBookmarkVerticalPadding;

    }
  } else {
    NOTREACHED();
  }

  return y;
}

- (ThemeProvider*)themeProvider {
  return [parentController_ themeProvider];
}

- (void)childFolderWillShow:(id<BookmarkButtonControllerProtocol>)child {
  // Do nothing.
}

- (void)childFolderWillClose:(id<BookmarkButtonControllerProtocol>)child {
  // Do nothing.
}

- (BookmarkBarFolderController*)folderController {
  return folderController_;
}

// Add a new folder controller as triggered by the given folder button.
- (void)addNewFolderControllerWithParentButton:(BookmarkButton*)parentButton {
  if (folderController_)
    [self closeBookmarkFolder:self];

  // Folder controller, like many window controllers, owns itself.
  folderController_ =
      [[BookmarkBarFolderController alloc] initWithParentButton:parentButton
                                               parentController:self
                                                  barController:barController_];
  [folderController_ showWindow:self];
}

- (void)openAll:(const BookmarkNode*)node
    disposition:(WindowOpenDisposition)disposition {
  [barController_ openAll:node disposition:disposition];
}

- (void)addButtonForNode:(const BookmarkNode*)node
                 atIndex:(NSInteger)buttonIndex {
  // Propose the frame for the new button.
  NSRect newButtonFrame = NSMakeRect(0, 0, 500, 500);  // Placeholder values.
  // When adding a button to an empty folder we must remove the 'empty'
  // placeholder button. This can be detected by checking for a parent
  // child count of 1.
  const BookmarkNode* parentNode = node->GetParent();
  if (parentNode->GetChildCount() == 1) {
    BookmarkButton* emptyButton = [buttons_ lastObject];
    newButtonFrame = [emptyButton frame];
    [emptyButton setDelegate:nil];
    [emptyButton removeFromSuperview];
    [buttons_ removeLastObject];
  }

  if (buttonIndex == -1 || buttonIndex > (NSInteger)[buttons_ count])
    buttonIndex = [buttons_ count];

  // Offset upward by one button height all buttons above insertion location.
  BookmarkButton* button = nil;  // Remember so it can be de-highlighted.
  for (NSInteger i = 0; i < buttonIndex; ++i) {
    button = [buttons_ objectAtIndex:i];
    // Remember this location in case it's the last button being moved
    // which is where the new button will be located.
    newButtonFrame = [button frame];
    NSRect buttonFrame = [button frame];
    buttonFrame.origin.y += bookmarks::kBookmarkBarHeight +
        bookmarks::kBookmarkVerticalPadding;
    [button setFrame:buttonFrame];
  }
  [[button cell] mouseExited:nil];  // De-highlight.
  BookmarkButton* newButton = [self makeButtonForNode:node
                                                frame:newButtonFrame];
  [buttons_ insertObject:newButton atIndex:buttonIndex];
  [mainView_ addSubview:newButton];

  // Close any child folder(s) which may still be open.
  [self closeBookmarkFolder:self];

  // Prelim height of the window.  We'll trim later as needed.
  int height = [self windowHeightForButtonCount:[buttons_ count]];
  [self adjustWindowForHeight:height];
}

// More code which essentially duplicates that of BookmarkBarController.
// TODO(mrossetti,jrg): http://crbug.com/35966
- (BOOL)addURLs:(NSArray*)urls withTitles:(NSArray*)titles at:(NSPoint)point {
  DCHECK([urls count] == [titles count]);
  BOOL nodesWereAdded = NO;
  // Figure out where these new bookmarks nodes are to be added.
  BookmarkButton* button = [self buttonForDroppingOnAtPoint:point];
  BookmarkModel* bookmarkModel = [self bookmarkModel];
  const BookmarkNode* destParent = NULL;
  int destIndex = 0;
  if ([button isFolder]) {
    destParent = [button bookmarkNode];
    // Drop it at the end.
    destIndex = [button bookmarkNode]->GetChildCount();
  } else {
    // Else we're dropping somewhere in the folder, so find the right spot.
    destParent = [parentButton_ bookmarkNode];
    destIndex = [self indexForDragToPoint:point];
    // Be careful if the number of buttons != number of nodes.
    destIndex += [[parentButton_ cell] startingChildIndex];
  }

  // Create and add the new bookmark nodes.
  size_t urlCount = [urls count];
  for (size_t i = 0; i < urlCount; ++i) {
    GURL gurl;
    const char* string = [[urls objectAtIndex:i] UTF8String];
    if (string)
      gurl = GURL(string);
    // We only expect to receive valid URLs.
    DCHECK(gurl.is_valid());
    if (gurl.is_valid()) {
      bookmarkModel->AddURL(destParent,
                            destIndex++,
                            base::SysNSStringToUTF16([titles objectAtIndex:i]),
                            gurl);
      nodesWereAdded = YES;
    }
  }
  return nodesWereAdded;
}

- (void)moveButtonFromIndex:(NSInteger)fromIndex toIndex:(NSInteger)toIndex {
  if (fromIndex != toIndex) {
    if (toIndex == -1)
      toIndex = [buttons_ count];
    BookmarkButton* movedButton = [buttons_ objectAtIndex:fromIndex];
    [buttons_ removeObjectAtIndex:fromIndex];
    NSRect movedFrame = [movedButton frame];
    NSPoint toOrigin = movedFrame.origin;
    [movedButton setHidden:YES];
    if (fromIndex < toIndex) {
      BookmarkButton* targetButton = [buttons_ objectAtIndex:toIndex - 1];
      toOrigin = [targetButton frame].origin;
      for (NSInteger i = fromIndex; i < toIndex; ++i) {
        BookmarkButton* button = [buttons_ objectAtIndex:i];
        NSRect frame = [button frame];
        frame.origin.y += bookmarks::kBookmarkBarHeight +
            bookmarks::kBookmarkVerticalPadding;
        [button setFrameOrigin:frame.origin];
      }
    } else {
      BookmarkButton* targetButton = [buttons_ objectAtIndex:toIndex];
      toOrigin = [targetButton frame].origin;
      for (NSInteger i = fromIndex - 1; i >= toIndex; --i) {
        BookmarkButton* button = [buttons_ objectAtIndex:i];
        NSRect buttonFrame = [button frame];
        buttonFrame.origin.y -= bookmarks::kBookmarkBarHeight +
            bookmarks::kBookmarkVerticalPadding;
        [button setFrameOrigin:buttonFrame.origin];
      }
    }
    [buttons_ insertObject:movedButton atIndex:toIndex];
    [movedButton setFrameOrigin:toOrigin];
    [movedButton setHidden:NO];
  }
}

// TODO(jrg): Refactor BookmarkBarFolder common code. http://crbug.com/35966
- (void)removeButton:(NSInteger)buttonIndex animate:(BOOL)animate {
  // TODO(mrossetti): Get disappearing animation to work. http://crbug.com/42360
  BookmarkButton* oldButton = [buttons_ objectAtIndex:buttonIndex];
  NSPoint poofPoint = [oldButton screenLocationForRemoveAnimation];

  // If a hover-open is pending, cancel it.
  if (oldButton == buttonThatMouseIsIn_) {
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    buttonThatMouseIsIn_ = nil;
  }

  // Deleting a button causes rearrangement that enables us to lose a
  // mouse-exited event.  This problem doesn't appear to exist with
  // other keep-menu-open options (e.g. add folder).  Since the
  // showsBorderOnlyWhileMouseInside uses a tracking area, simple
  // tricks (e.g. sending an extra mouseExited: to the button) don't
  // fix the problem.
  // http://crbug.com/54324
  for (NSButton* button in buttons_.get()) {
    if ([button showsBorderOnlyWhileMouseInside]) {
      [button setShowsBorderOnlyWhileMouseInside:NO];
      [button setShowsBorderOnlyWhileMouseInside:YES];
    }
  }

  [oldButton setDelegate:nil];
  [oldButton removeFromSuperview];
  if (animate && !ignoreAnimations_)
    NSShowAnimationEffect(NSAnimationEffectDisappearingItemDefault, poofPoint,
                          NSZeroSize, nil, nil, nil);
  [buttons_ removeObjectAtIndex:buttonIndex];
  for (NSInteger i = 0; i < buttonIndex; ++i) {
    BookmarkButton* button = [buttons_ objectAtIndex:i];
    NSRect buttonFrame = [button frame];
    buttonFrame.origin.y -= bookmarks::kBookmarkButtonHeight +
      bookmarks::kBookmarkVerticalPadding;
    [button setFrame:buttonFrame];
  }
  // Search for and adjust submenus, if necessary.
  NSInteger buttonCount = [buttons_ count];
  if (buttonCount) {
    BookmarkButton* subButton = [folderController_ parentButton];
    for (NSInteger i = buttonIndex; i < buttonCount; ++i) {
      BookmarkButton* aButton = [buttons_ objectAtIndex:i];
      // If this button is showing its menu then we need to move the menu, too.
      if (aButton == subButton)
        [folderController_ offsetFolderMenuWindow:NSMakeSize(0.0,
         bookmarks::kBookmarkBarHeight)];
    }
  } else {
    // If all nodes have been removed from this folder then add in the
    // 'empty' placeholder button.
    NSRect buttonFrame =
        NSMakeRect(bookmarks::kBookmarkSubMenuHorizontalPadding,
                   bookmarks::kBookmarkButtonHeight -
                   (bookmarks::kBookmarkBarHeight -
                    bookmarks::kBookmarkVerticalPadding),
                   bookmarks::kDefaultBookmarkWidth,
                   (bookmarks::kBookmarkBarHeight -
                    2 * bookmarks::kBookmarkVerticalPadding));
    BookmarkButton* button = [self makeButtonForNode:nil
                                               frame:buttonFrame];
    [buttons_ addObject:button];
    [mainView_ addSubview:button];
    buttonCount = 1;
  }

  // Propose a height for the window.  We'll trim later as needed.
  [self adjustWindowForHeight:[self windowHeightForButtonCount:buttonCount]];
}

- (id<BookmarkButtonControllerProtocol>)controllerForNode:
    (const BookmarkNode*)node {
  // See if we are holding this node, otherwise see if it is in our
  // hierarchy of visible folder menus.
  if ([parentButton_ bookmarkNode] == node)
    return self;
  return [folderController_ controllerForNode:node];
}

#pragma mark TestingAPI Only

- (void)setIgnoreAnimations:(BOOL)ignore {
  ignoreAnimations_ = ignore;
}

- (BookmarkButton*)buttonThatMouseIsIn {
  return buttonThatMouseIsIn_;
}

@end  // BookmarkBarFolderController
