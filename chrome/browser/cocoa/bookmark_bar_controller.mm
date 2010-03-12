// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#include "app/l10n_util_mac.h"
#include "app/resource_bundle.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#import "chrome/browser/browser_theme_provider.h"
#import "chrome/browser/cocoa/background_gradient_view.h"
#import "chrome/browser/cocoa/bookmark_bar_bridge.h"
#import "chrome/browser/cocoa/bookmark_bar_constants.h"
#import "chrome/browser/cocoa/bookmark_bar_folder_controller.h"
#import "chrome/browser/cocoa/bookmark_bar_folder_window.h"
#import "chrome/browser/cocoa/bookmark_bar_toolbar_view.h"
#import "chrome/browser/cocoa/bookmark_bar_view.h"
#import "chrome/browser/cocoa/bookmark_button.h"
#import "chrome/browser/cocoa/bookmark_button_cell.h"
#import "chrome/browser/cocoa/bookmark_editor_controller.h"
#import "chrome/browser/cocoa/bookmark_menu.h"
#import "chrome/browser/cocoa/bookmark_menu_cocoa_controller.h"
#import "chrome/browser/cocoa/bookmark_name_folder_controller.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#import "chrome/browser/cocoa/event_utils.h"
#import "chrome/browser/cocoa/menu_button.h"
#import "chrome/browser/cocoa/themed_window.h"
#import "chrome/browser/cocoa/toolbar_controller.h"
#import "chrome/browser/cocoa/view_resizer.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/common/pref_names.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/mozilla/include/NSPasteboard+Utils.h"

// Bookmark bar state changing and animations
//
// The bookmark bar has three real states: "showing" (a normal bar attached to
// the toolbar), "hidden", and "detached" (pretending to be part of the web
// content on the NTP). It can, or at least should be able to, animate between
// these states. There are several complications even without animation:
//  - The placement of the bookmark bar is done by the BWC, and it needs to know
//    the state in order to place the bookmark bar correctly (immediately below
//    the toolbar when showing, below the infobar when detached).
//  - The "divider" (a black line) needs to be drawn by either the toolbar (when
//    the bookmark bar is hidden or detached) or by the bookmark bar (when it is
//    showing). It should not be drawn by both.
//  - The toolbar needs to vertically "compress" when the bookmark bar is
//    showing. This ensures the proper display of both the bookmark bar and the
//    toolbar, and gives a padded area around the bookmark bar items for right
//    clicks, etc.
//
// Our model is that the BWC controls us and also the toolbar. We try not to
// talk to the browser nor the toolbar directly, instead centralizing control in
// the BWC. The key method by which the BWC controls us is
// |-updateAndShowNormalBar:showDetachedBar:withAnimation:|. This invokes state
// changes, and at appropriate times we request that the BWC do things for us
// via either the resize delegate or our general delegate. If the BWC needs any
// information about what it should do, or tell the toolbar to do, it can then
// query us back (e.g., |-isShownAs...|, |-getDesiredToolbarHeightCompression|,
// |-toolbarDividerOpacity|, etc.).
//
// Animation-related complications:
//  - Compression of the toolbar is touchy during animation. It must not be
//    compressed while the bookmark bar is animating to/from showing (from/to
//    hidden), otherwise it would look like the bookmark bar's contents are
//    sliding out of the controls inside the toolbar. As such, we have to make
//    sure that the bookmark bar is shown at the right location and at the
//    right height (at various points in time).
//  - Showing the divider is also complicated during animation between hidden
//    and showing. We have to make sure that the toolbar does not show the
//    divider despite the fact that it's not compressed. The exception to this
//    is at the beginning/end of the animation when the toolbar is still
//    uncompressed but the bookmark bar has height 0. If we're not careful, we
//    get a flicker at this point.
//  - We have to ensure that we do the right thing if we're told to change state
//    while we're running an animation. The generic/easy thing to do is to jump
//    to the end state of our current animation, and (if the new state change
//    again involves an animation) begin the new animation. We can do better
//    than that, however, and sometimes just change the current animation to go
//    to the new end state (e.g., by "reversing" the animation in the showing ->
//    hidden -> showing case). We also have to ensure that demands to
//    immediately change state are always honoured.
//
// Pointers to animation logic:
//  - |-moveToVisualState:withAnimation:| starts animations, deciding which ones
//    we know how to handle.
//  - |-doBookmarkBarAnimation| has most of the actual logic.
//  - |-getDesiredToolbarHeightCompression| and |-toolbarDividerOpacity| contain
//    related logic.
//  - The BWC's |-layoutSubviews| needs to know how to position things.
//  - The BWC should implement |-bookmarkBar:didChangeFromState:toState:| and
//    |-bookmarkBar:willAnimateFromState:toState:| in order to inform the
//    toolbar of required changes.

NSString* kBookmarkButtonDragType = @"ChromiumBookmarkButtonDragType";

namespace {

// Overlap (in pixels) between the toolbar and the bookmark bar (when showing in
// normal mode).
const CGFloat kBookmarkBarOverlap = 5.0;

// Duration of the bookmark bar animations.
const NSTimeInterval kBookmarkBarAnimationDuration = 0.12;

}  // namespace

@interface BookmarkBarController(Private)

// Determines the appropriate state for the given situation.
+ (bookmarks::VisualState)visualStateToShowNormalBar:(BOOL)showNormalBar
                                     showDetachedBar:(BOOL)showDetachedBar;

// Moves to the given next state (from the current state), possibly animating.
// If |animate| is NO, it will stop any running animation and jump to the given
// state. If YES, it may either (depending on implementation) jump to the end of
// the current animation and begin the next one, or stop the current animation
// mid-flight and animate to the next state.
- (void)moveToVisualState:(bookmarks::VisualState)nextVisualState
            withAnimation:(BOOL)animate;

// Return the backdrop to the bookmark bar as various types.
- (BackgroundGradientView*)backgroundGradientView;
- (AnimatableView*)animatableView;

// Create buttons for all items in the given bookmark node tree.
// Modifies self->buttons_.  Do not add more buttons than will fit on the view.
- (void)addNodesToButtonList:(const BookmarkNode*)node;

// Puts stuff into the final visual state without animating, stopping a running
// animation if necessary.
- (void)finalizeVisualState;

// Stops any current animation in its tracks (midway).
- (void)stopCurrentAnimation;

// Show/hide the bookmark bar.
// if |animate| is YES, the changes are made using the animator; otherwise they
// are made immediately.
- (void)showBookmarkBarWithAnimation:(BOOL)animate;

// Handles animating the resize of the content view. Returns YES if it handled
// the animation, NO if not (and hence it should be done instantly).
- (BOOL)doBookmarkBarAnimation;

// Returns the index in the model for a drag of the given button to the given
// location; currently, only the x-coordinate of |point| is considered. I
// reserve the right to check for errors, in which case this would return
// negative value; callers should check for this.
- (int)indexForDragOfButton:(BookmarkButton*)sourceButton
                    toPoint:(NSPoint)point;

// Copies the given bookmark node to the given pasteboard, declaring appropriate
// types (to paste a URL with a title).
- (void)copyBookmarkNode:(const BookmarkNode*)node
            toPasteboard:(NSPasteboard*)pboard;

- (void)addNode:(const BookmarkNode*)child toMenu:(NSMenu*)menu;
- (void)addFolderNode:(const BookmarkNode*)node toMenu:(NSMenu*)menu;
- (void)tagEmptyMenu:(NSMenu*)menu;
- (void)clearMenuTagMap;
- (int)preferredHeight;
- (void)addNonBookmarkButtonsToView;
- (void)addButtonsToView;
- (void)centerNoItemsLabel;
- (NSImage*)getFavIconForNode:(const BookmarkNode*)node;
- (void)setNodeForBarMenu;

- (void)watchForClickOutside:(BOOL)watch;

@end

@implementation BookmarkBarController

@synthesize visualState = visualState_;
@synthesize lastVisualState = lastVisualState_;
@synthesize delegate = delegate_;

- (id)initWithBrowser:(Browser*)browser
         initialWidth:(float)initialWidth
             delegate:(id<BookmarkBarControllerDelegate>)delegate
       resizeDelegate:(id<ViewResizer>)resizeDelegate {
  if ((self = [super initWithNibName:@"BookmarkBar"
                              bundle:mac_util::MainAppBundle()])) {
    // Initialize to an invalid state.
    visualState_ = bookmarks::kInvalidState;
    lastVisualState_ = bookmarks::kInvalidState;

    browser_ = browser;
    initialWidth_ = initialWidth;
    bookmarkModel_ = browser_->profile()->GetBookmarkModel();
    buttons_.reset([[NSMutableArray alloc] init]);
    delegate_ = delegate;
    resizeDelegate_ = resizeDelegate;

    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    folderImage_.reset([rb.GetNSImageNamed(IDR_BOOKMARK_BAR_FOLDER) retain]);
    defaultImage_.reset([rb.GetNSImageNamed(IDR_DEFAULT_FAVICON) retain]);

    // Register for theme changes.
    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserver:self
                      selector:@selector(themeDidChangeNotification:)
                          name:kBrowserThemeDidChangeNotification
                        object:nil];

    // This call triggers an awakeFromNib, which builds the bar, which
    // might uses folderImage_.  So make sure it happens after
    // folderImage_ is loaded.
    [[self animatableView] setResizeDelegate:resizeDelegate];
  }
  return self;
}

- (void)dealloc {
  // We better stop any in-flight animation if we're being killed.
  [[self animatableView] stopAnimation];

  // Remove our view from its superview so it doesn't attempt to reference
  // it when the controller is gone.
  //TODO(dmaclach): Remove -- http://crbug.com/25845
  [[self view] removeFromSuperview];

  // Be sure there is no dangling pointer.
  if ([[self view] respondsToSelector:@selector(setController:)])
    [[self view] performSelector:@selector(setController:) withObject:nil];

  // For safety, make sure the buttons can no longer call us.
  for (BookmarkButton* button in buttons_.get()) {
    [button setDelegate:nil];
    [button setTarget:nil];
    [button setAction:nil];
  }

  bridge_.reset(NULL);
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [self watchForClickOutside:NO];
  [super dealloc];
}

// Adapt appearance of buttons to the current theme. Called after
// theme changes, or when our view is added to the view hierarchy.
// Oddly, the view pings us instead of us pinging our view.  This is
// because our trigger is an [NSView viewWillMoveToWindow:], which the
// controller doesn't normally know about.  Otherwise we don't have
// access to the theme before we know what window we will be on.
- (void)updateTheme:(ThemeProvider*)themeProvider {
  if (!themeProvider)
    return;
  NSColor* color =
      themeProvider->GetNSColor(BrowserThemeProvider::COLOR_BOOKMARK_TEXT,
                                true);
  for (BookmarkButton* button in buttons_.get()) {
    BookmarkButtonCell* cell = [button cell];
    [cell setTextColor:color];
  }
  [[otherBookmarksButton_ cell] setTextColor:color];
}

// Exposed purely for testing.
- (BookmarkBarFolderController*)folderController {
  return folderController_;
}

// Called after the current theme has changed.
- (void)themeDidChangeNotification:(NSNotification*)aNotification {
  ThemeProvider* themeProvider =
      static_cast<ThemeProvider*>([[aNotification object] pointerValue]);
  [self updateTheme:themeProvider];
}

- (void)awakeFromNib {
  // We default to NOT open, which means height=0.
  DCHECK([[self view] isHidden]);  // Hidden so it's OK to change.

  // Set our initial height to zero, since that is what the superview
  // expects.  We will resize ourselves open later if needed.
  [[self view] setFrame:NSMakeRect(0, 0, initialWidth_, 0)];

  // We are enabled by default.
  barIsEnabled_ = YES;

  DCHECK([offTheSideButton_ attachedMenu]);

  // To make life happier when the bookmark bar is floating, the chevron is a
  // child of the button view.
  [offTheSideButton_ removeFromSuperview];
  [buttonView_ addSubview:offTheSideButton_];

  // Copy the bar menu so we know if it's from the bar or a folder.
  // Then we set its represented item to be the bookmark bar.
  buttonFolderContextMenu_.reset([[[self view] menu] copy]);

  // When resized we may need to add new buttons, or remove them (if
  // no longer visible), or add/remove the "off the side" menu.
  [[self view] setPostsFrameChangedNotifications:YES];
  [[NSNotificationCenter defaultCenter]
    addObserver:self
       selector:@selector(frameDidChange)
           name:NSViewFrameDidChangeNotification
         object:[self view]];

  // Don't pass ourself along (as 'self') until our init is completely
  // done.  Thus, this call is (almost) last.
  bridge_.reset(new BookmarkBarBridge(self, bookmarkModel_));
}

// Called by our main view (a BookmarkBarView) when it gets moved to a
// window.  We perform operations which need to know the relevant
// window (e.g. watch for a window close) so they can't be performed
// earlier (such as in awakeFromNib).
- (void)viewDidMoveToWindow {
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];

  // Remove any existing notifications before registering for new ones.
  [defaultCenter removeObserver:self
                           name:NSWindowWillCloseNotification
                         object:nil];
  [defaultCenter removeObserver:self
                           name:NSWindowDidResignKeyNotification
                         object:nil];

  [defaultCenter addObserver:self
                    selector:@selector(parentWindowWillClose:)
                        name:NSWindowWillCloseNotification
                      object:[[self view] window]];
  [defaultCenter addObserver:self
                    selector:@selector(parentWindowDidResignKey:)
                        name:NSWindowDidResignKeyNotification
                      object:[[self view] window]];
}

// (Private) Method is the same as [self view], but is provided to be explicit.
- (BackgroundGradientView*)backgroundGradientView {
  DCHECK([[self view] isKindOfClass:[BackgroundGradientView class]]);
  return (BackgroundGradientView*)[self view];
}

// (Private) Method is the same as [self view], but is provided to be explicit.
- (AnimatableView*)animatableView {
  DCHECK([[self view] isKindOfClass:[AnimatableView class]]);
  return (AnimatableView*)[self view];
}

// Position the off-the-side chevron to the left of the otherBookmarks button.
- (void)positionOffTheSideButton {
  NSRect frame = [offTheSideButton_ frame];
  if (otherBookmarksButton_.get()) {
    frame.origin.x = ([otherBookmarksButton_ frame].origin.x -
                      (frame.size.width +
                       bookmarks::kBookmarkHorizontalPadding));
    [offTheSideButton_ setFrame:frame];
  }
}

// Check if we should enable or disable the off-the-side chevron.
// Assumes that buttons which don't fit in the parent view are removed
// from it.
- (void)showOrHideOffTheSideButton {
  int bookmarkChildren = bookmarkModel_->GetBookmarkBarNode()->GetChildCount();
  if (bookmarkChildren > bookmarkBarDisplayedButtons_) {
    [offTheSideButton_ setHidden:NO];
  } else {
    [offTheSideButton_ setHidden:YES];
  }
}

- (BOOL)offTheSideButtonIsHidden {
  return [offTheSideButton_ isHidden];
}

// Called when our controlled frame has changed size.
- (void)frameDidChange {
  if (!bookmarkModel_->IsLoaded())
    return;
  CGFloat width = NSWidth([[self view] frame]);
  if (width > savedFrameWidth_) {
    savedFrameWidth_ = width;
    [self clearBookmarkBar];
    [self addNodesToButtonList:bookmarkModel_->GetBookmarkBarNode()];
    [self updateTheme:[[[self view] window] themeProvider]];
    [self addButtonsToView];
  }
  [self positionOffTheSideButton];
  [self addButtonsToView];
  [self showOrHideOffTheSideButton];
  [self centerNoItemsLabel];
}

// Close all bookmark folders.  "Folder" here is the fake menu for
// bookmark folders, not a button context menu.
- (void)closeAllBookmarkFolders {
  [self watchForClickOutside:NO];
  [[folderController_ window] close];
  folderController_ = nil;
}

- (void)closeBookmarkFolder:(id)sender {
  // We're the top level, so close one means close them all.
  [self closeAllBookmarkFolders];
}

- (BookmarkModel*)bookmarkModel {
  return bookmarkModel_;
}

// NSNotificationCenter callback.
- (void)parentWindowWillClose:(NSNotification*)notification {
  [self closeAllBookmarkFolders];
}

// NSNotificationCenter callback.
- (void)parentWindowDidResignKey:(NSNotification*)notification {
  [self closeAllBookmarkFolders];
}

// BookmarkButtonDelegate protocol implementation.  When menus are
// "active" (e.g. you clicked to open one), moving the mouse over
// another folder button should close the 1st and open the 2nd (like
// real menus).  We detect and act here.
- (void)mouseEnteredButton:(id)sender event:(NSEvent*)event {
  DCHECK([sender isKindOfClass:[BookmarkButton class]]);
  // If nothing is open, do nothing.  Different from
  // BookmarkBarFolderController since we default to NOT being enabled.
  if (!folderController_)
    return;

  // From here down: same logic as BookmarkBarFolderController.
  // TODO(jrg): find a way to share these 4 non-comment lines?
  // http://crbug.com/35966

  // If already opened, then we exited but re-entered the button, so do nothing.
  if ([folderController_ parentButton] == sender)
    return;
  // Else open a new one if it makes sense to do so.
  if ([sender bookmarkNode]->is_folder())
    [self openBookmarkFolderFromButton:sender];
}

// BookmarkButtonDelegate protocol implementation.
- (void)mouseExitedButton:(id)sender event:(NSEvent*)event {
  // Don't care; do nothing.
  // This is different behavior that the folder menus.
}

// Begin (or end) watching for a click outside this window.  Unlike
// normal NSWindows, bookmark folder "fake menu" windows do not become
// key or main.  Thus, traditional notification (e.g. WillResignKey)
// won't work.  Our strategy is to watch (at the app level) for a
// "click outside" these windows to detect when they logically lose
// focus.
- (void)watchForClickOutside:(BOOL)watch {
  CrApplication* app = static_cast<CrApplication*>([NSApplication
                                                       sharedApplication]);
  DCHECK([app isKindOfClass:[CrApplication class]]);
  if (watch) {
    if (!watchingForClickOutside_)
      [app addEventHook:self];
  } else {
    if (watchingForClickOutside_)
      [app removeEventHook:self];
  }
  watchingForClickOutside_ = watch;
}

// Implementation of CrApplicationEventHookProtocol.
// NOT an override of a standard Cocoa call made to NSViewControllers.
- (void)hookForEvent:(NSEvent*)theEvent {
  if ([self isEventAClickOutside:theEvent]) {
    [self watchForClickOutside:NO];
    [self closeAllBookmarkFolders];
  }
}

// Return YES if the event represents a "click outside" of the area we
// are watching.  At this time we are watching the area that includes
// all popup bookmark folder windows.
- (BOOL)isEventAClickOutside:(NSEvent*)event {
  NSWindow* eventWindow = [event window];
  NSWindow* myWindow = [[self view] window];
  switch ([event type]) {
    case NSLeftMouseDown:
    case NSRightMouseDown:
      // If a click in my window and NOT in the bookmark bar,
      // then is a click outside.
      if (eventWindow == myWindow &&
          ![[[eventWindow contentView] hitTest:[event locationInWindow]]
              isDescendantOf:[self view]]) {
          return YES;
      }
      // If a click in a bookmark bar folder window and that isn't
      // one of my bookmark bar folders, YES is click outside.
      if ([eventWindow isKindOfClass:[BookmarkBarFolderWindow
                                          class]] &&
          [eventWindow parentWindow] != myWindow) {
        return YES;
      }
      break;
    default:
      break;
  }
  return NO;
}

// Keep the "no items" label centered in response to a frame size change.
- (void)centerNoItemsLabel {
  // Note that this computation is done in the parent's coordinate system, which
  // is unflipped. Also, we want the label to be a fixed distance from the
  // bottom, so that it slides up properly (on animating to hidden).
  NSPoint parentOrigin = [buttonView_ bounds].origin;
  [[buttonView_ noItemTextfield] setFrameOrigin:NSMakePoint(
      parentOrigin.x + bookmarks::kNoBookmarksHorizontalOffset,
      parentOrigin.y + bookmarks::kNoBookmarksVerticalOffset)];
}

// Change the layout of the bookmark bar's subviews in response to a visibility
// change (e.g., show or hide the bar) or style change (attached or floating).
- (void)layoutSubviews {
  NSRect frame = [[self view] frame];
  NSRect buttonViewFrame = NSMakeRect(0, 0, NSWidth(frame), NSHeight(frame));

  // The state of our morph (if any); 1 is total bubble, 0 is the regular bar.
  CGFloat morph = [self detachedMorphProgress];

  // Add padding to the detached bookmark bar.
  buttonViewFrame = NSInsetRect(buttonViewFrame,
                                morph * bookmarks::kNTPBookmarkBarPadding,
                                morph * bookmarks::kNTPBookmarkBarPadding);

  [buttonView_ setFrame:buttonViewFrame];
}

// (Private)
- (void)showBookmarkBarWithAnimation:(BOOL)animate {
  if (animate) {
    // If |-doBookmarkBarAnimation| does the animation, we're done.
    if ([self doBookmarkBarAnimation])
      return;

    // Else fall through and do the change instantly.
  }

  // Set our height.
  [resizeDelegate_ resizeView:[self view]
                    newHeight:[self preferredHeight]];

  // Only show the divider if showing the normal bookmark bar.
  BOOL showsDivider = [self isInState:bookmarks::kShowingState];
  [[self backgroundGradientView] setShowsDivider:showsDivider];

  // Make sure we're shown.
  [[self view] setHidden:![self isVisible]];

  // Update everything else.
  [self layoutSubviews];
  [self frameDidChange];
}

// (Private)
- (BOOL)doBookmarkBarAnimation {
  if ([self isAnimatingFromState:bookmarks::kHiddenState
                         toState:bookmarks::kShowingState]) {
    [[self backgroundGradientView] setShowsDivider:YES];
    [[self view] setHidden:NO];
    AnimatableView* view = [self animatableView];
    // Height takes into account the extra height we have since the toolbar
    // only compresses when we're done.
    [view animateToNewHeight:(bookmarks::kBookmarkBarHeight -
                              kBookmarkBarOverlap)
                    duration:kBookmarkBarAnimationDuration];
  } else if ([self isAnimatingFromState:bookmarks::kShowingState
                                toState:bookmarks::kHiddenState]) {
    [[self backgroundGradientView] setShowsDivider:YES];
    [[self view] setHidden:NO];
    AnimatableView* view = [self animatableView];
    [view animateToNewHeight:0
                    duration:kBookmarkBarAnimationDuration];
  } else if ([self isAnimatingFromState:bookmarks::kShowingState
                                toState:bookmarks::kDetachedState]) {
    [[self backgroundGradientView] setShowsDivider:YES];
    [[self view] setHidden:NO];
    AnimatableView* view = [self animatableView];
    [view animateToNewHeight:bookmarks::kNTPBookmarkBarHeight
                    duration:kBookmarkBarAnimationDuration];
  } else if ([self isAnimatingFromState:bookmarks::kDetachedState
                                toState:bookmarks::kShowingState]) {
    [[self backgroundGradientView] setShowsDivider:YES];
    [[self view] setHidden:NO];
    AnimatableView* view = [self animatableView];
    // Height takes into account the extra height we have since the toolbar
    // only compresses when we're done.
    [view animateToNewHeight:(bookmarks::kBookmarkBarHeight -
                              kBookmarkBarOverlap)
                    duration:kBookmarkBarAnimationDuration];
  } else {
    // Oops! An animation we don't know how to handle.
    return NO;
  }

  return YES;
}

// We don't change a preference; we only change visibility. Preference changing
// (global state) is handled in |BrowserWindowCocoa::ToggleBookmarkBar()|. We
// simply update based on what we're told.
- (void)updateVisibility {
  [self showBookmarkBarWithAnimation:NO];
}

- (void)setBookmarkBarEnabled:(BOOL)enabled {
  if (enabled != barIsEnabled_) {
    barIsEnabled_ = enabled;
    [self updateVisibility];
  }
}

- (CGFloat)getDesiredToolbarHeightCompression {
  // Some special cases....
  if (!barIsEnabled_)
    return 0;

  if ([self isAnimationRunning]) {
    // No toolbar compression when animating between hidden and showing, nor
    // between showing and detached.
    if ([self isAnimatingBetweenState:bookmarks::kHiddenState
                             andState:bookmarks::kShowingState] ||
        [self isAnimatingBetweenState:bookmarks::kShowingState
                             andState:bookmarks::kDetachedState])
      return 0;

    // If we ever need any other animation cases, code would go here.
  }

  return [self isInState:bookmarks::kShowingState] ? kBookmarkBarOverlap : 0;
}

- (CGFloat)toolbarDividerOpacity {
  // Some special cases....
  if ([self isAnimationRunning]) {
    // In general, the toolbar shouldn't show a divider while we're animating
    // between showing and hidden. The exception is when our height is < 1, in
    // which case we can't draw it. It's all-or-nothing (no partial opacity).
    if ([self isAnimatingBetweenState:bookmarks::kHiddenState
                             andState:bookmarks::kShowingState])
      return (NSHeight([[self view] frame]) < 1) ? 1 : 0;

    // The toolbar should show the divider when animating between showing and
    // detached (but opacity will vary).
    if ([self isAnimatingBetweenState:bookmarks::kShowingState
                             andState:bookmarks::kDetachedState])
      return static_cast<CGFloat>([self detachedMorphProgress]);

    // If we ever need any other animation cases, code would go here.
  }

  // In general, only show the divider when it's in the normal showing state.
  return [self isInState:bookmarks::kShowingState] ? 0 : 1;
}

- (BOOL)addURLs:(NSArray*)urls withTitles:(NSArray*)titles at:(NSPoint)point {
  // TODO(jrg): Support drops on folders etc
  // TODO(jrg): Use |point|.
  DCHECK([urls count] == [titles count]);
  const BookmarkNode* node = bookmarkModel_->GetBookmarkBarNode();

  for (size_t i = 0; i < [urls count]; ++i) {
    bookmarkModel_->AddURL(
        node,
        node->GetChildCount(),
        base::SysNSStringToWide([titles objectAtIndex:i]),
        GURL([[urls objectAtIndex:i] UTF8String]));
  }
  return YES;
}

- (int)indexForDragOfButton:(BookmarkButton*)sourceButton
                    toPoint:(NSPoint)point {
  DCHECK([sourceButton isKindOfClass:[BookmarkButton class]]);

  // Identify which buttons we are between.  For now, assume a button
  // location is at the center point of its view, and that an exact
  // match means "place before".
  // TODO(jrg): revisit position info based on UI team feedback.
  // dropLocation is in bar local coordinates.
  NSPoint dropLocation =
      [[self view] convertPoint:point
                       fromView:[[[self view] window] contentView]];
  BookmarkButton* buttonToTheRightOfDraggedButton = nil;
  for (BookmarkButton* button in buttons_.get()) {
    CGFloat midpoint = NSMidX([button frame]);
    if (dropLocation.x <= midpoint) {
      buttonToTheRightOfDraggedButton = button;
      break;
    }
  }
  if (buttonToTheRightOfDraggedButton) {
    const BookmarkNode* afterNode =
        [buttonToTheRightOfDraggedButton bookmarkNode];
    DCHECK(afterNode);
    return afterNode->GetParent()->IndexOfChild(afterNode);
  }

  // If nothing is to my right I am at the end!
  return [buttons_ count];
}

- (BOOL)dragButton:(BookmarkButton*)sourceButton
                to:(NSPoint)point
              copy:(BOOL)copy {
  DCHECK([sourceButton isKindOfClass:[BookmarkButton class]]);

  const BookmarkNode* sourceNode = [sourceButton bookmarkNode];
  DCHECK(sourceNode);

  int destIndex = [self indexForDragOfButton:sourceButton toPoint:point];
  if (destIndex >= 0 && sourceNode) {
    // Our destination parent is not sourceNode->GetParent()!
    if (copy) {
      bookmarkModel_->Copy(sourceNode,
                           bookmarkModel_->GetBookmarkBarNode(),
                           destIndex);
    } else {
      bookmarkModel_->Move(sourceNode,
                           bookmarkModel_->GetBookmarkBarNode(),
                           destIndex);
    }
  } else {
    NOTREACHED();
  }

  // Movement of a node triggers observers (like us) to rebuild the
  // bar so we don't have to do so explicitly.

  return YES;
}

// Find something like std::is_between<T>?  I can't believe one doesn't exist.
static BOOL ValueInRangeInclusive(CGFloat low, CGFloat value, CGFloat high) {
  return ((value >= low) && (value <= high));
}

// Return the proposed drop target for a hover open button from the
// given array, or nil if none.  We use this for distinguishing
// between a hover-open candidate or drop-indicator draw.
// Helper for buttonForDroppingOnAtPoint:.
// Get UI review on "middle half" ness.
// http://crbug.com/36276
- (BookmarkButton*)buttonForDroppingOnAtPoint:(NSPoint)point
                                    fromArray:(NSArray*)array {
  for (BookmarkButton* button in array) {
    // Break early if we've gone too far.
    if (NSMinX([button frame]) > point.x)
      return nil;
    // Careful -- this only applies to the bar with horiz buttons.
    // Intentionally NOT using NSPointInRect() so that scrolling into
    // a submenu doesn't cause it to be closed.
    if (ValueInRangeInclusive(NSMinX([button frame]),
                              point.x,
                              NSMaxX([button frame]))) {
      // Over a button but let's be a little more specific (make sure
      // it's over the middle half, not just over it.)
      NSRect frame = [button frame];
      NSRect middleHalfOfButton = NSInsetRect(frame, frame.size.width / 4, 0);
      if (ValueInRangeInclusive(NSMinX(middleHalfOfButton),
                                point.x,
                                NSMaxX(middleHalfOfButton))) {
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

// Return the proposed drop target for a hover open button, or nil if
// none.  Works with both the bookmark buttons and the "Other
// Bookmarks" button.  Point is in [self view] coordinates.
- (BookmarkButton*)buttonForDroppingOnAtPoint:(NSPoint)point {
  BookmarkButton* button = [self buttonForDroppingOnAtPoint:point
                                                  fromArray:buttons_.get()];
  // One more chance -- try "Other Bookmarks".
  // This is different than BookmarkBarFolderController.
  if (!button) {
    NSArray* array = [NSArray arrayWithObject:otherBookmarksButton_];
    button = [self buttonForDroppingOnAtPoint:point
                                    fromArray:array];
  }
  return button;
}

// TODO(jrg): much of this logic is duped with
// [BookmarkBarFolderController draggingEntered:] except when noted.
// http://crbug.com/35966
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info {
  NSPoint point = [info draggingLocation];
  BookmarkButton* button = [self buttonForDroppingOnAtPoint:point];
  if ([button isFolder]) {
    if (hoverButton_ == button) {
      return NSDragOperationMove;  // already open or timed to open
    }
    if (hoverButton_) {
      // Oops, another one triggered or open.
      [NSObject cancelPreviousPerformRequestsWithTarget:[hoverButton_
                                                            target]];
      // Unlike BookmarkBarFolderController, we do not delay the close
      // of the previous one.  Given the lack of diagonal movement,
      // there is no need, and it feels awkward to do so.  See
      // comments about kDragHoverCloseDelay in
      // bookmark_bar_folder_controller.mm for more details.
      [[hoverButton_ target] closeBookmarkFolder:hoverButton_];
      hoverButton_.reset();
    }
    hoverButton_.reset([button retain]);
    [[hoverButton_ target]
            performSelector:@selector(openBookmarkFolderFromButton:)
                 withObject:hoverButton_
                 afterDelay:bookmarks::kDragHoverOpenDelay];
  }
  if (!button) {
    if (hoverButton_) {
      [NSObject cancelPreviousPerformRequestsWithTarget:[hoverButton_ target]];
      [[hoverButton_ target] closeBookmarkFolder:hoverButton_];
      hoverButton_.reset();
    }
  }

  // Thrown away but kept to be consistent with the draggingEntered: interface.
  return NSDragOperationMove;
}

- (void)draggingExited:(id<NSDraggingInfo>)info {
  // NOT the same as a cancel --> we may have moved the mouse into the submenu.
  if (hoverButton_) {
    [NSObject cancelPreviousPerformRequestsWithTarget:[hoverButton_ target]];
    hoverButton_.reset();
  }
}

// Return YES if we should show the drop indicator, else NO.
- (BOOL)shouldShowIndicatorShownForPoint:(NSPoint)point {
  return ![self buttonForDroppingOnAtPoint:point];
}

// Return the x position for a drop indicator.
- (CGFloat)indicatorPosForDragOfButton:(BookmarkButton*)sourceButton
                               toPoint:(NSPoint)point {
  CGFloat x = 0;
  int destIndex = [self indexForDragOfButton:sourceButton toPoint:point];
  int numButtons = static_cast<int>([buttons_ count]);

  // If it's a drop strictly between existing buttons ...
  if (destIndex >= 0 && destIndex < numButtons) {
    // ... put the indicator right between the buttons.
    BookmarkButton* button =
        [buttons_ objectAtIndex:static_cast<NSUInteger>(destIndex)];
    DCHECK(button);
    NSRect buttonFrame = [button frame];
    x = buttonFrame.origin.x - 0.5 * bookmarks::kBookmarkHorizontalPadding;

  // If it's a drop at the end (past the last button, if there are any) ...
  } else if (destIndex == numButtons) {
    // and if it's past the last button ...
    if (numButtons > 0) {
      // ... find the last button, and put the indicator to its right.
      BookmarkButton* button =
          [buttons_ objectAtIndex:static_cast<NSUInteger>(destIndex - 1)];
      DCHECK(button);
      NSRect buttonFrame = [button frame];
      x = buttonFrame.origin.x + buttonFrame.size.width +
          0.5 * bookmarks::kBookmarkHorizontalPadding;

    // Otherwise, put it right at the beginning.
    } else {
      x = 0.5 * bookmarks::kBookmarkHorizontalPadding;
    }
  } else {
    NOTREACHED();
  }

  return x;
}

// Return the parent window for all BookmarkBarFolderController windows.
- (NSWindow*)parentWindow {
  return [[self view] window];
}

- (int)currentTabContentsHeight {
  return browser_->GetSelectedTabContents() ?
      browser_->GetSelectedTabContents()->view()->GetContainerSize().height() :
      0;
}

- (ThemeProvider*)themeProvider {
  return browser_->profile()->GetThemeProvider();
}

- (void)childFolderWillShow:(id<BookmarkButtonControllerProtocol>)child {
  // Lock bar visibility, forcing the overlay to stay open when in fullscreen
  // mode.
  BrowserWindowController* browserController =
      [BrowserWindowController browserWindowControllerForView:[self view]];
  [browserController lockBarVisibilityForOwner:child
                                 withAnimation:NO
                                         delay:NO];
}

- (void)childFolderWillClose:(id<BookmarkButtonControllerProtocol>)child {
  // Release bar visibility, allowing the overlay to close if in fullscreen
  // mode.
  BrowserWindowController* browserController =
      [BrowserWindowController browserWindowControllerForView:[self view]];
  [browserController releaseBarVisibilityForOwner:child
                                    withAnimation:NO
                                            delay:NO];
}


// Enable or disable items.  We are the menu delegate for both the bar
// and for bookmark folder buttons.
- (BOOL)validateUserInterfaceItem:(id)item {
  if (![item isKindOfClass:[NSMenuItem class]])
    return YES;

  BookmarkNode* node = [self nodeFromMenuItem:item];

  // If this is the bar menu, we only have things to do if there are
  // buttons.  If this is a folder button menu, we only have things to
  // do if the folder has items.
  NSMenu* menu = [item menu];
  BOOL thingsToDo = NO;
  if (menu == [[self view] menu]) {
    thingsToDo = [buttons_ count] ? YES : NO;
  } else {
    if (node && node->is_folder() && node->GetChildCount()) {
      thingsToDo = YES;
    }
  }

  // Disable openAll* if we have nothing to do.
  SEL action = [item action];
  if ((!thingsToDo) &&
      ((action == @selector(openAllBookmarks:)) ||
       (action == @selector(openAllBookmarksNewWindow:)) ||
       (action == @selector(openAllBookmarksIncognitoWindow:)))) {
    return NO;
  }

  if ((action == @selector(editBookmark:)) ||
      (action == @selector(deleteBookmark:))) {
    // Don't allow edit/delete of the bar node, or of "Other Bookmarks"
    if ((node == nil) ||
        (node == bookmarkModel_->other_node()) ||
        (node == bookmarkModel_->GetBookmarkBarNode())) {
      return NO;
    }
  }

  // If this is an incognito window, don't allow "open in incognito".
  if ((action == @selector(openBookmarkInIncognitoWindow:)) ||
      (action == @selector(openAllBookmarksIncognitoWindow:))) {
    if (browser_->profile()->IsOffTheRecord()) {
      return NO;
    }
  }

  // Enabled by default.
  return YES;
}

// Actually open the URL.  This is the last chance for a unit test to
// override.
- (void)openURL:(GURL)url disposition:(WindowOpenDisposition)disposition {
  BrowserList::GetLastActive()->OpenURL(url, GURL(), disposition,
                                        PageTransition::AUTO_BOOKMARK);
}

- (IBAction)openBookmark:(id)sender {
  [self closeAllBookmarkFolders];
  DCHECK([sender respondsToSelector:@selector(bookmarkNode)]);
  const BookmarkNode* node = [sender bookmarkNode];
  WindowOpenDisposition disposition =
      event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  [self openURL:node->GetURL() disposition:disposition];
}

// Given a NSMenuItem tag, return the appropriate bookmark node id.
- (int64)nodeIdFromMenuTag:(int32)tag {
  return menuTagMap_[tag];
}

// Create and return a new tag for the given node id.
- (int32)menuTagFromNodeId:(int64)menuid {
  int tag = seedId_++;
  menuTagMap_[tag] = menuid;
  return tag;
}

- (void)clearMenuTagMap {
  seedId_ = 0;
  menuTagMap_.clear();
}

- (int)preferredHeight {
  DCHECK(![self isAnimationRunning]);

  if (!barIsEnabled_)
    return 0;

  switch (visualState_) {
    case bookmarks::kShowingState:
      return bookmarks::kBookmarkBarHeight;
    case bookmarks::kDetachedState:
      return bookmarks::kNTPBookmarkBarHeight;
    case bookmarks::kHiddenState:
      return 0;
    case bookmarks::kInvalidState:
    default:
      NOTREACHED();
      return 0;
  }
}

// Recursively add the given bookmark node and all its children to
// menu, one menu item per node.
- (void)addNode:(const BookmarkNode*)child toMenu:(NSMenu*)menu {
  NSString* title = [BookmarkMenuCocoaController menuTitleForNode:child];
  NSMenuItem* item = [[[NSMenuItem alloc] initWithTitle:title
                                                 action:nil
                                          keyEquivalent:@""] autorelease];
  [menu addItem:item];
  [item setImage:[self getFavIconForNode:child]];
  if (child->is_folder()) {
    NSMenu* submenu = [[[NSMenu alloc] initWithTitle:title] autorelease];
    [menu setSubmenu:submenu forItem:item];
    if (child->GetChildCount()) {
      [self addFolderNode:child toMenu:submenu];  // potentially recursive
    } else {
      [self tagEmptyMenu:submenu];
    }
  } else {
    [item setTarget:self];
    [item setAction:@selector(openBookmarkMenuItem:)];
    [item setTag:[self menuTagFromNodeId:child->id()]];
    // Add a tooltip
    std::string url_string = child->GetURL().possibly_invalid_spec();
    NSString* tooltip = [NSString stringWithFormat:@"%@\n%s",
                                  base::SysWideToNSString(child->GetTitle()),
                                  url_string.c_str()];
    [item setToolTip:tooltip];
  }
}

// Empty menus are odd; if empty, add something to look at.
// Matches windows behavior.
- (void)tagEmptyMenu:(NSMenu*)menu {
  NSString* empty_menu_title = l10n_util::GetNSString(IDS_MENU_EMPTY_SUBMENU);
  [menu addItem:[[[NSMenuItem alloc] initWithTitle:empty_menu_title
                                            action:NULL
                                     keyEquivalent:@""] autorelease]];
}

// Add the children of the given bookmark node (and their children...)
// to menu, one menu item per node.
- (void)addFolderNode:(const BookmarkNode*)node toMenu:(NSMenu*)menu {
  for (int i = 0; i < node->GetChildCount(); i++) {
    const BookmarkNode* child = node->GetChild(i);
    [self addNode:child toMenu:menu];
  }
}

// Return an autoreleased NSMenu that represents the given bookmark
// folder node.
- (NSMenu *)menuForFolderNode:(const BookmarkNode*)node {
  if (!node->is_folder())
    return nil;
  NSString* title = base::SysWideToNSString(node->GetTitle());
  NSMenu* menu = [[[NSMenu alloc] initWithTitle:title] autorelease];
  [self addFolderNode:node toMenu:menu];

  if (![menu numberOfItems]) {
    [self tagEmptyMenu:menu];
  }
  return menu;
}

// Heuristics:
// 1: click and hold (without moving much): opens it
// 2: click and release (normal button push): opens it.
// This is called on the second one.
//
// Called from a Folder bookmark button to open the folder.
- (IBAction)openBookmarkFolderFromButton:(id)sender {
  DCHECK(sender);
  if (folderController_) {
    // closeAllBookmarkFolders sets folderController_ to nil
    // so we need the SAME check to happen first.
    BOOL same = ([folderController_ parentButton] == sender);
    [self closeAllBookmarkFolders];
    // If click on same folder, close it and be done.
    // Else we clicked on a different folder so more work to do.
    if (same)
      return;
  }

  // Folder controller, like many window controllers, owns itself.
  folderController_ = [[BookmarkBarFolderController alloc]
                            initWithParentButton:sender
                                parentController:self];
  [folderController_ showWindow:self];
  [self watchForClickOutside:YES];
}

// Rebuild the off-the-side menu.
- (void)buildOffTheSideMenuIfNeeded {
  NSMenu* menu = [self offTheSideMenu];
  DCHECK(menu);

  // Only rebuild if needed.  We determine we need a rebuild when the
  // bookmark bar is cleared of buttons.
  if (!needToRebuildOffTheSideMenu_)
    return;
  needToRebuildOffTheSideMenu_ = YES;

  // Remove old menu items (backwards order is as good as any); leave the
  // blank one at position 0 (see menu_button.h).
  for (NSInteger i = [menu numberOfItems] - 1; i >= 1 ; i--)
    [menu removeItemAtIndex:i];

  // Add items corresponding to buttons which aren't displayed.  Since
  // we build the buttons in the same order as the bookmark bar child
  // count, we have a clear hint as to where to begin.
  const BookmarkNode* barNode = bookmarkModel_->GetBookmarkBarNode();
  for (int i = bookmarkBarDisplayedButtons_;
       i < barNode->GetChildCount(); i++) {
    const BookmarkNode* child = barNode->GetChild(i);
    [self addNode:child toMenu:menu];
  }
}

// Get the off-the-side menu.
- (NSMenu*)offTheSideMenu {
  return [offTheSideButton_ attachedMenu];
}

// Called by any menus which have set us as their delegate (right now just the
// off-the-side menu).  This is the trigger for a delayed rebuild.
- (void)menuNeedsUpdate:(NSMenu*)menu {
  if (menu == [self offTheSideMenu])
    [self buildOffTheSideMenuIfNeeded];
}

// As a convention we set the menu's delegate to be the button's cell
// so we can easily obtain bookmark info.  Convention applied in
// -[BookmarkButtonCell menu].

- (IBAction)openBookmarkInNewForegroundTab:(id)sender {
  BookmarkNode* node = [self nodeFromMenuItem:sender];
  [self openURL:node->GetURL() disposition:NEW_FOREGROUND_TAB];
}

- (IBAction)openBookmarkInNewWindow:(id)sender {
  BookmarkNode* node = [self nodeFromMenuItem:sender];
  [self openURL:node->GetURL() disposition:NEW_WINDOW];
}

- (IBAction)openBookmarkInIncognitoWindow:(id)sender {
  BookmarkNode* node = [self nodeFromMenuItem:sender];
  [self openURL:node->GetURL() disposition:OFF_THE_RECORD];
}

- (IBAction)editBookmark:(id)sender {
  BookmarkNode* node = [self nodeFromMenuItem:sender];

  if (node->is_folder()) {
    BookmarkNameFolderController* controller =
        [[BookmarkNameFolderController alloc]
          initWithParentWindow:[[self view] window]
                       profile:browser_->profile()
                          node:node];
    [controller runAsModalSheet];
    return;
  }

  // There is no real need to jump to a platform-common routine at
  // this point (which just jumps back to objc) other than consistency
  // across platforms.
  //
  // TODO(jrg): identify when we NO_TREE.  I can see it in the code
  // for the other platforms but can't find a way to trigger it in the
  // UI.
  BookmarkEditor::Show([[self view] window],
                       browser_->profile(),
                       node->GetParent(),
                       BookmarkEditor::EditDetails(node),
                       BookmarkEditor::SHOW_TREE,
                       nil);
}

- (IBAction)copyBookmark:(id)sender {
  BookmarkNode* node = [self nodeFromMenuItem:sender];
  NSPasteboard* pboard = [NSPasteboard generalPasteboard];
  [self copyBookmarkNode:node
            toPasteboard:pboard];
}

- (IBAction)deleteBookmark:(id)sender {
  BookmarkNode* node = [self nodeFromMenuItem:sender];
  bookmarkModel_->Remove(node->GetParent(),
                         node->GetParent()->IndexOfChild(node));
  // TODO(jrg): don't close; rebuild.
  // http://crbug.com/36614
  [self closeAllBookmarkFolders];
}

// An ObjC version of bookmark_utils::OpenAllImpl().
- (void)openBookmarkNodesRecursive:(BookmarkNode*)node
                       disposition:(WindowOpenDisposition)disposition {
  for (int i = 0; i < node->GetChildCount(); i++) {
    BookmarkNode* child = node->GetChild(i);
    if (child->is_url()) {
      [self openURL:child->GetURL() disposition:disposition];
      // We revert to a basic disposition in case the initial
      // disposition opened a new window.
      disposition = NEW_BACKGROUND_TAB;
    } else {
      [self openBookmarkNodesRecursive:child disposition:disposition];
    }
  }
}

// Return the BookmarkNode associated with the given NSMenuItem.
- (BookmarkNode*)nodeFromMenuItem:(id)sender {
  BookmarkMenu* menu = (BookmarkMenu*)[sender menu];
  if ([menu isKindOfClass:[BookmarkMenu class]])
    return const_cast<BookmarkNode*>([menu node]);
  return NULL;
}

- (IBAction)openAllBookmarks:(id)sender {
  BookmarkNode* node = [self nodeFromMenuItem:sender];
  [self openBookmarkNodesRecursive:node disposition:NEW_FOREGROUND_TAB];
  UserMetrics::RecordAction("OpenAllBookmarks", browser_->profile());
}

- (IBAction)openAllBookmarksNewWindow:(id)sender {
  BookmarkNode* node = [self nodeFromMenuItem:sender];
  [self openBookmarkNodesRecursive:node disposition:NEW_WINDOW];
  UserMetrics::RecordAction("OpenAllBookmarksNewWindow", browser_->profile());
}

- (IBAction)openAllBookmarksIncognitoWindow:(id)sender {
  BookmarkNode* node = [self nodeFromMenuItem:sender];
  [self openBookmarkNodesRecursive:node disposition:OFF_THE_RECORD];
  UserMetrics::RecordAction("OpenAllBookmarksIncognitoWindow", browser_->profile());
}

// May be called from the bar or from a folder button.
// If called from a button, that button becomes the parent.
- (IBAction)addPage:(id)sender {
  const BookmarkNode* parent = [self nodeFromMenuItem:sender];
  if (!parent)
    parent = bookmarkModel_->GetBookmarkBarNode();
  BookmarkEditor::Show([[self view] window],
                       browser_->profile(),
                       parent,
                       BookmarkEditor::EditDetails(),
                       BookmarkEditor::SHOW_TREE,
                       nil);
}

// Might be from the context menu over the bar OR over a button.
- (IBAction)addFolder:(id)sender {
  BookmarkNameFolderController* controller =
    [[BookmarkNameFolderController alloc]
      initWithParentWindow:[[self view] window]
                   profile:browser_->profile()
                      node:NULL];
  [controller runAsModalSheet];
}

- (BookmarkBarView*)buttonView {
  return buttonView_;
}

// Delete all buttons (bookmarks, chevron, "other bookmarks") from the
// bookmark bar; reset knowledge of bookmarks.
- (void)clearBookmarkBar {
  [buttons_ makeObjectsPerformSelector:@selector(removeFromSuperview)];
  [buttons_ removeAllObjects];
  [self clearMenuTagMap];
  needToRebuildOffTheSideMenu_ = YES;
  bookmarkBarDisplayedButtons_ = 0;

  // Make sure there are no stale pointers in the pasteboard.  This
  // can be important if a bookmark is deleted (via bookmark sync)
  // while in the middle of a drag.  The "drag completed" code
  // (e.g. [BookmarkBarView performDragOperationForBookmark:]) is
  // careful enough to bail if there is no data found at "drop" time.
  //
  // Unfortunately the clearContents selector is 10.6 only.  The best
  // we can do is make sure something else is present in place of the
  // stale bookmark.
  NSPasteboard* pboard = [NSPasteboard pasteboardWithName:NSDragPboard];
  [pboard declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:self];
  [pboard setString:@"" forType:NSStringPboardType];
}

// Return an autoreleased NSCell suitable for a bookmark button.
// TODO(jrg): move much of the cell config into the BookmarkButtonCell class.
- (NSCell*)cellForBookmarkNode:(const BookmarkNode*)node {
  BookmarkButtonCell* cell =
    [[[BookmarkButtonCell alloc] initTextCell:nil]
      autorelease];
  DCHECK(cell);
  [cell setBookmarkNode:node];

  if (node) {
    NSString* title = base::SysWideToNSString(node->GetTitle());
    NSImage* image = [self getFavIconForNode:node];
    [cell setBookmarkCellText:title image:image];
    if (node->is_folder())
      [cell setMenu:buttonFolderContextMenu_];
    else
      [cell setMenu:buttonContextMenu_];
  } else {
    [cell setEmpty:YES];
    [cell setBookmarkCellText:l10n_util::GetNSString(IDS_MENU_EMPTY_SUBMENU)
                        image:nil];
  }

  // Note: a quirk of setting a cell's text color is that it won't work
  // until the cell is associated with a button, so we can't theme the cell yet.

  return cell;
}

// Return an appropriate width for the given bookmark button cell.
// The "+2" is needed because, sometimes, Cocoa is off by a tad.
// Example: for a bookmark named "Moma" or "SFGate", it is one pixel
// too small.  For "FBL" it is 2 pixels too small.
// For a bookmark named "SFGateFooWoo", it is just fine.
- (CGFloat)widthForBookmarkButtonCell:(NSCell*)cell {
  CGFloat desired = [cell cellSize].width + 2;
  return std::min(desired, bookmarks::kDefaultBookmarkWidth);
}

// Returns a frame appropriate for the given bookmark cell, suitable
// for creating an NSButton that will contain it.  |xOffset| is the X
// offset for the frame; it is increased to be an appropriate X offset
// for the next button.
- (NSRect)frameForBookmarkButtonFromCell:(NSCell*)cell
                                 xOffset:(int*)xOffset {
  NSRect bounds = [buttonView_ bounds];
  // TODO(erg,jrg): There used to be an if statement here, comparing the height
  // to 0. This essentially broke sizing because we're dealing with floats and
  // the height wasn't precisely zero. The previous author wrote that they were
  // doing this because of an animator, but we are not doing animations for beta
  // so we do not care.
  bounds.size.height = bookmarks::kBookmarkButtonHeight;

  NSRect frame = NSInsetRect(bounds,
                             bookmarks::kBookmarkHorizontalPadding,
                             bookmarks::kBookmarkVerticalPadding);
  frame.size.width = [self widthForBookmarkButtonCell:cell];

  // Add an X offset based on what we've already done
  frame.origin.x += *xOffset;

  // And up the X offset for next time.
  *xOffset = NSMaxX(frame);

  return frame;
}

// A bookmark button's contents changed.  Check for growth
// (e.g. increase the width up to the maximum).  If we grew, move
// other bookmark buttons over.
- (void)checkForBookmarkButtonGrowth:(NSButton*)button {
  NSRect frame = [button frame];
  CGFloat desiredSize = [self widthForBookmarkButtonCell:[button cell]];
  CGFloat delta = desiredSize - frame.size.width;
  if (delta) {
    frame.size.width = desiredSize;
    [button setFrame:frame];
    for (NSButton* button in buttons_.get()) {
      NSRect buttonFrame = [button frame];
      if (buttonFrame.origin.x > frame.origin.x) {
        buttonFrame.origin.x += delta;
        [button setFrame:buttonFrame];
      }
    }
  }
  // We may have just crossed a threshold to enable the off-the-side
  // button.
  [self showOrHideOffTheSideButton];
}

- (IBAction)openBookmarkMenuItem:(id)sender {
  int64 tag = [self nodeIdFromMenuTag:[sender tag]];
  const BookmarkNode* node = bookmarkModel_->GetNodeByID(tag);
  WindowOpenDisposition disposition =
      event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  [self openURL:node->GetURL() disposition:disposition];
}

// TODO(jrg): write a "build bar" so there is a nice spot for things
// like the contextual menu which is invoked when not over a
// bookmark.  On Safari that menu has a "new folder" option.
- (void)addNodesToButtonList:(const BookmarkNode*)node {
  BOOL hideNoItemWarning = node->GetChildCount() > 0;
  [[buttonView_ noItemTextfield] setHidden:hideNoItemWarning];

  CGFloat maxViewX = NSMaxX([[self view] bounds]);
  int xOffset = 0;
  for (int i = 0; i < node->GetChildCount(); i++) {
    const BookmarkNode* child = node->GetChild(i);

    NSCell* cell = [self cellForBookmarkNode:child];
    NSRect frame = [self frameForBookmarkButtonFromCell:cell xOffset:&xOffset];

    // Break early if the button is off the end of the parent view.
    if (NSMinX(frame) >= maxViewX)
      break;

    scoped_nsobject<BookmarkButton>
        button([[BookmarkButton alloc] initWithFrame:frame]);
    DCHECK(button.get());
    [buttons_ addObject:button];

    // [NSButton setCell:] warns to NOT use setCell: other than in the
    // initializer of a control.  However, we are using a basic
    // NSButton whose initializer does not take an NSCell as an
    // object.  To honor the assumed semantics, we do nothing with
    // NSButton between alloc/init and setCell:.
    [button setCell:cell];
    [button setDelegate:self];

    if (child->is_folder()) {
      [button setTarget:self];
      [button setAction:@selector(openBookmarkFolderFromButton:)];
    } else {
      // Make the button do something
      [button setTarget:self];
      [button setAction:@selector(openBookmark:)];
      // Add a tooltip.
      NSString* title = base::SysWideToNSString(child->GetTitle());
      std::string url_string = child->GetURL().possibly_invalid_spec();
      NSString* tooltip = [NSString stringWithFormat:@"%@\n%s", title,
                                    url_string.c_str()];
      [button setToolTip:tooltip];
    }
  }
}

// Add non-bookmark buttons to the view.  This includes the chevron
// and the "other bookmarks" button.  Technically "other bookmarks" is
// a bookmark button but it is treated specially.  Only needs to be
// called when these buttons are new or when the bookmark bar is
// cleared (e.g. on a loaded: call).  Unlike addButtonsToView below,
// we don't need to add/remove these dynamically in response to window
// resize.
- (void)addNonBookmarkButtonsToView {
  [buttonView_ addSubview:otherBookmarksButton_.get()];
  [buttonView_ addSubview:offTheSideButton_];
}

// Add bookmark buttons to the view only if they are completely
// visible and don't overlap the "other bookmarks".  Remove buttons
// which are clipped.  Called when building the bookmark bar and when
// the window resizes.
- (void)addButtonsToView {
  NSView* superview = nil;
  bookmarkBarDisplayedButtons_ = 0;
  for (NSButton* button in buttons_.get()) {
    superview = [button superview];
    if (NSMaxX([button frame]) <= NSMinX([offTheSideButton_ frame])) {
      if (!superview)
        [buttonView_ addSubview:button];
      bookmarkBarDisplayedButtons_++;
    } else {
      if (superview)
        [button removeFromSuperview];
    }
  }
}

// Create the button for "Other Bookmarks" on the right of the bar.
- (void)createOtherBookmarksButton {
  // Can't create this until the model is loaded, but only need to
  // create it once.
  if (otherBookmarksButton_.get())
    return;

  // TODO(jrg): remove duplicate code
  NSCell* cell = [self cellForBookmarkNode:bookmarkModel_->other_node()];
  int ignored = 0;
  NSRect frame = [self frameForBookmarkButtonFromCell:cell xOffset:&ignored];
  frame.origin.x = [[self buttonView] bounds].size.width - frame.size.width;
  frame.origin.x -= bookmarks::kBookmarkHorizontalPadding;
  BookmarkButton* button = [[BookmarkButton alloc] initWithFrame:frame];
  [button setDraggable:NO];
  otherBookmarksButton_.reset(button);

  // Peg at right; keep same height as bar.
  [button setAutoresizingMask:(NSViewMinXMargin)];
  [button setCell:cell];
  [button setDelegate:self];
  [button setTarget:self];
  [button setAction:@selector(openBookmarkFolderFromButton:)];
  [buttonView_ addSubview:button];

  // Now that it's here, move the chevron over.
  [self positionOffTheSideButton];
}

// TODO(jrg): for now this is brute force.
- (void)loaded:(BookmarkModel*)model {
  DCHECK(model == bookmarkModel_);
  if (!model->IsLoaded())
    return;
  // Else brute force nuke and build.
  savedFrameWidth_ = NSWidth([[self view] frame]);
  const BookmarkNode* node = model->GetBookmarkBarNode();
  [self clearBookmarkBar];
  [self addNodesToButtonList:node];
  [self createOtherBookmarksButton];
  [self updateTheme:[[[self view] window] themeProvider]];
  [self positionOffTheSideButton];
  [self addNonBookmarkButtonsToView];
  [self addButtonsToView];
  [self showOrHideOffTheSideButton];
  [self setNodeForBarMenu];
}

// Now that the model is loaded, set the bookmark bar root as the node
// represented by the bookmark bar (default, background) menu.
- (void)setNodeForBarMenu {
  const BookmarkNode* node = bookmarkModel_->GetBookmarkBarNode();
  BookmarkMenu* menu = static_cast<BookmarkMenu*>([[self view] menu]);
  [menu setRepresentedObject:[NSValue valueWithPointer:node]];
}

- (void)beingDeleted:(BookmarkModel*)model {
  [self clearBookmarkBar];
}

// TODO(jrg): for now this is brute force.
- (void)nodeMoved:(BookmarkModel*)model
        oldParent:(const BookmarkNode*)oldParent oldIndex:(int)oldIndex
        newParent:(const BookmarkNode*)newParent newIndex:(int)newIndex {
  [self loaded:model];
}

// TODO(jrg): for now this is brute force.
- (void)nodeAdded:(BookmarkModel*)model
           parent:(const BookmarkNode*)oldParent index:(int)index {
  [self loaded:model];
}

// TODO(jrg): for now this is brute force.
- (void)nodeRemoved:(BookmarkModel*)model
             parent:(const BookmarkNode*)oldParent index:(int)index {
  [self loaded:model];
}

// TODO(jrg): for now this is brute force.
- (void)nodeChanged:(BookmarkModel*)model
               node:(const BookmarkNode*)node {
  [self loaded:model];
}

// TODO(jrg): linear searching is bad.
// Need a BookmarkNode-->NSCell mapping.
//
// TODO(jrg): if the bookmark bar is open on launch, we see the
// buttons all placed, then "scooted over" as the favicons load.  If
// this looks bad I may need to change widthForBookmarkButtonCell to
// add space for an image even if not there on the assumption that
// favicons will eventually load.
- (void)nodeFavIconLoaded:(BookmarkModel*)model
                     node:(const BookmarkNode*)node {
  for (BookmarkButton* button in buttons_.get()) {
    const BookmarkNode* cellnode = [button bookmarkNode];
    if (cellnode == node) {
      [[button cell] setBookmarkCellText:nil
                                   image:[self getFavIconForNode:node]];
      // Adding an image means we might need more room for the
      // bookmark.  Test for it by growing the button (if needed)
      // and shifting everything else over.
      [self checkForBookmarkButtonGrowth:button];
    }
  }
}

// TODO(jrg): for now this is brute force.
- (void)nodeChildrenReordered:(BookmarkModel*)model
                         node:(const BookmarkNode*)node {
  [self loaded:model];
}

- (NSArray*)buttons {
  return buttons_.get();
}

- (NSButton*)offTheSideButton {
  return offTheSideButton_;
}

- (NSButton*)otherBookmarksButton {
  return otherBookmarksButton_.get();
}

- (NSImage*)getFavIconForNode:(const BookmarkNode*)node {
  if (node->is_folder())
    return folderImage_;

  const SkBitmap& favIcon = bookmarkModel_->GetFavIcon(node);
  if (!favIcon.isNull())
    return gfx::SkBitmapToNSImage(favIcon);

  return defaultImage_;
}

// Determines the appropriate state for the given situation.
+ (bookmarks::VisualState)visualStateToShowNormalBar:(BOOL)showNormalBar
                                     showDetachedBar:(BOOL)showDetachedBar {
  if (showNormalBar)
    return bookmarks::kShowingState;
  if (showDetachedBar)
    return bookmarks::kDetachedState;
  return bookmarks::kHiddenState;
}

- (void)moveToVisualState:(bookmarks::VisualState)nextVisualState
            withAnimation:(BOOL)animate {
  BOOL isAnimationRunning = [self isAnimationRunning];

  // No-op if the next state is the same as the "current" one, subject to the
  // following conditions:
  //  - no animation is running; or
  //  - an animation is running and |animate| is YES ([*] if it's NO, we'd want
  //    to cancel the animation and jump to the final state).
  if ((nextVisualState == visualState_) && (!isAnimationRunning || animate))
    return;

  // If an animation is running, we want to finalize it. Otherwise we'd have to
  // be able to animate starting from the middle of one type of animation. We
  // assume that animations that we know about can be "reversed".
  if (isAnimationRunning) {
    // Don't cancel if we're going to reverse the animation.
    if (nextVisualState != lastVisualState_) {
      [self stopCurrentAnimation];
      [self finalizeVisualState];
    }

    // If we're in case [*] above, we can stop here.
    if (nextVisualState == visualState_)
      return;
  }

  // Now update with the new state change.
  lastVisualState_ = visualState_;
  visualState_ = nextVisualState;

  // Animate only if told to and if bar is enabled.
  if (animate && barIsEnabled_) {
    // Take care of any animation cases we know how to handle.

    // We know how to handle hidden <-> normal, normal <-> detached....
    if ([self isAnimatingBetweenState:bookmarks::kHiddenState
                             andState:bookmarks::kShowingState] ||
        [self isAnimatingBetweenState:bookmarks::kShowingState
                             andState:bookmarks::kDetachedState]) {
      [delegate_ bookmarkBar:self willAnimateFromState:lastVisualState_
                                               toState:visualState_];
      [self showBookmarkBarWithAnimation:YES];
      return;
    }

    // If we ever need any other animation cases, code would go here.
    // Let any animation cases which we don't know how to handle fall through to
    // the unanimated case.
  }

  // Just jump to the state.
  [self finalizeVisualState];
}

// N.B.: |-moveToVisualState:...| will check if this should be a no-op or not.
- (void)updateAndShowNormalBar:(BOOL)showNormalBar
               showDetachedBar:(BOOL)showDetachedBar
                 withAnimation:(BOOL)animate {
  bookmarks::VisualState newVisualState =
      [BookmarkBarController visualStateToShowNormalBar:showNormalBar
                                        showDetachedBar:showDetachedBar];
  [self moveToVisualState:newVisualState
            withAnimation:animate];
}

// (Private)
- (void)finalizeVisualState {
  // We promise that our delegate that the variables will be finalized before
  // the call to |-bookmarkBar:didChangeFromState:toState:|.
  bookmarks::VisualState oldVisualState = lastVisualState_;
  lastVisualState_ = bookmarks::kInvalidState;

  // Notify our delegate.
  [delegate_ bookmarkBar:self didChangeFromState:oldVisualState
                                         toState:visualState_];

  // Update ourselves visually.
  [self updateVisibility];
}

// (Private)
- (void)stopCurrentAnimation {
  [[self animatableView] stopAnimation];
}

// (Private)
- (void)copyBookmarkNode:(const BookmarkNode*)node
            toPasteboard:(NSPasteboard*)pboard {
  if (!node) {
    NOTREACHED();
    return;
  }

  if (node->is_folder()) {
    // TODO(viettrungluu): I'm not sure what we should do, so just declare the
    // "additional" types we're given for now. Maybe we want to add a list of
    // URLs? Would we then have to recurse if there were subfolders?
    // In the meanwhile, we *must* set it to a known state. (If this survives to
    // a 10.6-only release, it can be replaced with |-clearContents|.)
    [pboard declareTypes:[NSArray array] owner:nil];
  } else {
    const std::string spec = node->GetURL().spec();
    NSString* url = base::SysUTF8ToNSString(spec);
    NSString* title = base::SysWideToNSString(node->GetTitle());
    [pboard declareURLPasteboardWithAdditionalTypes:[NSArray array]
                                              owner:nil];
    [pboard setDataForURL:url title:title];
  }
}

// Delegate method for |AnimatableView| (a superclass of
// |BookmarkBarToolbarView|).
- (void)animationDidEnd:(NSAnimation*)animation {
  [self finalizeVisualState];
}

// (BookmarkBarState protocol)
- (BOOL)isVisible {
  return barIsEnabled_ && (visualState_ == bookmarks::kShowingState ||
                           visualState_ == bookmarks::kDetachedState ||
                           lastVisualState_ == bookmarks::kShowingState ||
                           lastVisualState_ == bookmarks::kDetachedState);
}

// (BookmarkBarState protocol)
- (BOOL)isAnimationRunning {
  return lastVisualState_ != bookmarks::kInvalidState;
}

// (BookmarkBarState protocol)
- (BOOL)isInState:(bookmarks::VisualState)state {
  return visualState_ == state &&
         lastVisualState_ == bookmarks::kInvalidState;
}

// (BookmarkBarState protocol)
- (BOOL)isAnimatingToState:(bookmarks::VisualState)state {
  return visualState_ == state &&
         lastVisualState_ != bookmarks::kInvalidState;
}

// (BookmarkBarState protocol)
- (BOOL)isAnimatingFromState:(bookmarks::VisualState)state {
  return lastVisualState_ == state;
}

// (BookmarkBarState protocol)
- (BOOL)isAnimatingFromState:(bookmarks::VisualState)fromState
                     toState:(bookmarks::VisualState)toState {
  return lastVisualState_ == fromState && visualState_ == toState;
}

// (BookmarkBarState protocol)
- (BOOL)isAnimatingBetweenState:(bookmarks::VisualState)fromState
                       andState:(bookmarks::VisualState)toState {
  return (lastVisualState_ == fromState && visualState_ == toState) ||
         (visualState_ == fromState && lastVisualState_ == toState);
}

// (BookmarkBarState protocol)
- (CGFloat)detachedMorphProgress {
  if ([self isInState:bookmarks::kDetachedState]) {
    return 1;
  }
  if ([self isAnimatingToState:bookmarks::kDetachedState]) {
    return static_cast<CGFloat>(
        [[self animatableView] currentAnimationProgress]);
  }
  if ([self isAnimatingFromState:bookmarks::kDetachedState]) {
    return static_cast<CGFloat>(
        1 - [[self animatableView] currentAnimationProgress]);
  }
  return 0;
}

// (BookmarkButtonDelegate protocol)
- (void)fillPasteboard:(NSPasteboard*)pboard
       forDragOfButton:(BookmarkButton*)button {
  if (const BookmarkNode* node = [button bookmarkNode]) {
    // Put the bookmark information into the pasteboard, and then write our own
    // data for |kBookmarkButtonDragType|.
    [self copyBookmarkNode:node
              toPasteboard:pboard];
    [pboard addTypes:[NSArray arrayWithObject:kBookmarkButtonDragType]
               owner:nil];
    [pboard setData:[NSData dataWithBytes:&button length:sizeof(button)]
            forType:kBookmarkButtonDragType];
  } else {
    NOTREACHED();
  }
}

@end
