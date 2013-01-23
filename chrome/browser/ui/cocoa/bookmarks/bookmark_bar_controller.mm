// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_controller.h"

#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/metrics/histogram.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/themes/theme_service.h"
#import "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/chrome_pages.h"
#import "chrome/browser/ui/cocoa/background_gradient_view.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_bridge.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_folder_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_folder_window.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_toolbar_view.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_view.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_button.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_button_cell.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_editor_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_folder_target.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_cocoa_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_name_folder_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/event_utils.h"
#import "chrome/browser/ui/cocoa/menu_button.h"
#import "chrome/browser/ui/cocoa/presentation_mode_controller.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#import "chrome/browser/ui/cocoa/view_resizer.h"
#include "chrome/browser/ui/search/search.h"
#include "chrome/browser/ui/search/search_ui.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/mac/nsimage_cache.h"

using content::OpenURLParams;
using content::Referrer;
using content::UserMetricsAction;
using content::WebContents;

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
// |-updateState:ChangeType:|. This invokes state changes, and at appropriate
// times we request that the BWC do things for us via either the resize delegate
// or our general delegate. If the BWC needs any information about what it
// should do, or tell the toolbar to do, it can then query us back (e.g.,
// |-isShownAs...|, |-getDesiredToolbarHeightCompression|,
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
//  - |-moveToState:withAnimation:| starts animations, deciding which ones we
//    know how to handle.
//  - |-doBookmarkBarAnimation| has most of the actual logic.
//  - |-getDesiredToolbarHeightCompression| and |-toolbarDividerOpacity| contain
//    related logic.
//  - The BWC's |-layoutSubviews| needs to know how to position things.
//  - The BWC should implement |-bookmarkBar:didChangeFromState:toState:| and
//    |-bookmarkBar:willAnimateFromState:toState:| in order to inform the
//    toolbar of required changes.

namespace {

// Overlap (in pixels) between the toolbar and the bookmark bar (when showing in
// normal mode).
const CGFloat kBookmarkBarOverlap = 3.0;

// Duration of the bookmark bar animations.
const NSTimeInterval kBookmarkBarAnimationDuration = 0.12;

void RecordAppLaunch(Profile* profile, GURL url) {
  DCHECK(profile->GetExtensionService());
  if (!profile->GetExtensionService()->IsInstalledApp(url))
    return;

  AppLauncherHandler::RecordAppLaunchType(
      extension_misc::APP_LAUNCH_BOOKMARK_BAR);
}

}  // namespace

@interface BookmarkBarController(Private)

// Moves to the given next state (from the current state), possibly animating.
// If |animate| is NO, it will stop any running animation and jump to the given
// state. If YES, it may either (depending on implementation) jump to the end of
// the current animation and begin the next one, or stop the current animation
// mid-flight and animate to the next state.
- (void)moveToState:(BookmarkBar::State)nextState
      withAnimation:(BOOL)animate;

// Return the backdrop to the bookmark bar as various types.
- (BackgroundGradientView*)backgroundGradientView;
- (AnimatableView*)animatableView;

// Create buttons for all items in the given bookmark node tree.
// Modifies self->buttons_.  Do not add more buttons than will fit on the view.
- (void)addNodesToButtonList:(const BookmarkNode*)node;

// Create an autoreleased button appropriate for insertion into the bookmark
// bar. Update |xOffset| with the offset appropriate for the subsequent button.
- (BookmarkButton*)buttonForNode:(const BookmarkNode*)node
                         xOffset:(int*)xOffset;

// Puts stuff into the final state without animating, stopping a running
// animation if necessary.
- (void)finalizeState;

// Stops any current animation in its tracks (midway).
- (void)stopCurrentAnimation;

// Show/hide the bookmark bar.
// if |animate| is YES, the changes are made using the animator; otherwise they
// are made immediately.
- (void)showBookmarkBarWithAnimation:(BOOL)animate;

// Handles animating the resize of the content view. Returns YES if it handled
// the animation, NO if not (and hence it should be done instantly).
- (BOOL)doBookmarkBarAnimation;

// |point| is in the base coordinate system of the destination window;
// it comes from an id<NSDraggingInfo>. |copy| is YES if a copy is to be
// made and inserted into the new location while leaving the bookmark in
// the old location, otherwise move the bookmark by removing from its old
// location and inserting into the new location.
- (BOOL)dragBookmark:(const BookmarkNode*)sourceNode
                  to:(NSPoint)point
                copy:(BOOL)copy;

// Returns the index in the model for a drag to the location given by
// |point|. This is determined by finding the first button before the center
// of which |point| falls, scanning left to right. Note that, currently, only
// the x-coordinate of |point| is considered. Though not currently implemented,
// we may check for errors, in which case this would return negative value;
// callers should check for this.
- (int)indexForDragToPoint:(NSPoint)point;

// Add or remove buttons to/from the bar until it is filled but not overflowed.
- (void)redistributeButtonsOnBarAsNeeded;

// Determine the nature of the bookmark bar contents based on the number of
// buttons showing. If too many then show the off-the-side list, if none
// then show the no items label.
- (void)reconfigureBookmarkBar;

- (void)addNode:(const BookmarkNode*)child toMenu:(NSMenu*)menu;
- (void)addFolderNode:(const BookmarkNode*)node toMenu:(NSMenu*)menu;
- (void)tagEmptyMenu:(NSMenu*)menu;
- (void)clearMenuTagMap;
- (int)preferredHeight;
- (void)addNonBookmarkButtonsToView;
- (void)addButtonsToView;
- (void)centerNoItemsLabel;
- (void)setNodeForBarMenu;
- (void)watchForExitEvent:(BOOL)watch;
- (void)resetAllButtonPositionsWithAnimation:(BOOL)animate;

@end

@implementation BookmarkBarController

@synthesize state = state_;
@synthesize lastState = lastState_;
@synthesize isAnimationRunning = isAnimationRunning_;
@synthesize delegate = delegate_;
@synthesize isEmpty = isEmpty_;
@synthesize stateAnimationsEnabled = stateAnimationsEnabled_;
@synthesize innerContentAnimationsEnabled = innerContentAnimationsEnabled_;

- (id)initWithBrowser:(Browser*)browser
         initialWidth:(CGFloat)initialWidth
             delegate:(id<BookmarkBarControllerDelegate>)delegate
       resizeDelegate:(id<ViewResizer>)resizeDelegate {
  if ((self = [super initWithNibName:@"BookmarkBar"
                              bundle:base::mac::FrameworkBundle()])) {
    state_ = BookmarkBar::HIDDEN;
    lastState_ = BookmarkBar::HIDDEN;

    browser_ = browser;
    initialWidth_ = initialWidth;
    bookmarkModel_ = BookmarkModelFactory::GetForProfile(browser_->profile());
    buttons_.reset([[NSMutableArray alloc] init]);
    delegate_ = delegate;
    resizeDelegate_ = resizeDelegate;
    folderTarget_.reset([[BookmarkFolderTarget alloc] initWithController:self]);

    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    folderImage_.reset(
        rb.GetNativeImageNamed(IDR_BOOKMARK_BAR_FOLDER).CopyNSImage());
    defaultImage_.reset(
        rb.GetNativeImageNamed(IDR_DEFAULT_FAVICON).CopyNSImage());

    innerContentAnimationsEnabled_ = YES;
    stateAnimationsEnabled_ = YES;

    // Register for theme changes, bookmark button pulsing, ...
    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserver:self
                      selector:@selector(themeDidChangeNotification:)
                          name:kBrowserThemeDidChangeNotification
                        object:nil];
    [defaultCenter addObserver:self
                      selector:@selector(pulseBookmarkNotification:)
                          name:bookmark_button::kPulseBookmarkButtonNotification
                        object:nil];

    // This call triggers an awakeFromNib, which builds the bar, which
    // might uses folderImage_.  So make sure it happens after
    // folderImage_ is loaded.
    [[self animatableView] setResizeDelegate:resizeDelegate];
  }
  return self;
}

- (void)pulseBookmarkNotification:(NSNotification*)notification {
  NSDictionary* dict = [notification userInfo];
  const BookmarkNode* node = NULL;
  NSValue *value = [dict objectForKey:bookmark_button::kBookmarkKey];
  DCHECK(value);
  if (value)
    node = static_cast<const BookmarkNode*>([value pointerValue]);
  NSNumber* number = [dict
                       objectForKey:bookmark_button::kBookmarkPulseFlagKey];
  DCHECK(number);
  BOOL doPulse = number ? [number boolValue] : NO;

  // 3 cases:
  // button on the bar: flash it
  // button in "other bookmarks" folder: flash other bookmarks
  // button in "off the side" folder: flash the chevron
  for (BookmarkButton* button in [self buttons]) {
    if ([button bookmarkNode] == node) {
      [button setIsContinuousPulsing:doPulse];
      return;
    }
  }
  if ([otherBookmarksButton_ bookmarkNode] == node) {
    [otherBookmarksButton_ setIsContinuousPulsing:doPulse];
    return;
  }
  if (node->parent() == bookmarkModel_->bookmark_bar_node()) {
    [offTheSideButton_ setIsContinuousPulsing:doPulse];
    return;
  }

  NOTREACHED() << "no bookmark button found to pulse!";
}

- (void)dealloc {
  // Clear delegate so it doesn't get called during stopAnimation.
  [[self animatableView] setResizeDelegate:nil];

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
  [self watchForExitEvent:NO];
  [super dealloc];
}

- (void)awakeFromNib {
  // We default to NOT open, which means height=0.
  DCHECK([[self view] isHidden]);  // Hidden so it's OK to change.

  // Set our initial height to zero, since that is what the superview
  // expects.  We will resize ourselves open later if needed.
  [[self view] setFrame:NSMakeRect(0, 0, initialWidth_, 0)];

  // Complete init of the "off the side" button, as much as we can.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  [offTheSideButton_ setImage:
        rb.GetNativeImageNamed(IDR_BOOKMARK_BAR_CHEVRONS).ToNSImage()];
  [offTheSideButton_.draggableButton setDraggable:NO];
  [offTheSideButton_.draggableButton setActsOnMouseDown:YES];

  // We are enabled by default.
  barIsEnabled_ = YES;

  // Remember the original sizes of the 'no items' and 'import bookmarks'
  // fields to aid in resizing when the window frame changes.
  originalNoItemsRect_ = [[buttonView_ noItemTextfield] frame];
  originalImportBookmarksRect_ = [[buttonView_ importBookmarksButton] frame];

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

  // Watch for things going to or from fullscreen.
  [[NSNotificationCenter defaultCenter]
    addObserver:self
       selector:@selector(willEnterOrLeaveFullscreen:)
           name:kWillEnterFullscreenNotification
         object:nil];
  [[NSNotificationCenter defaultCenter]
    addObserver:self
       selector:@selector(willEnterOrLeaveFullscreen:)
           name:kWillLeaveFullscreenNotification
         object:nil];

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
                           name:NSWindowDidResignMainNotification
                         object:nil];

  [defaultCenter addObserver:self
                    selector:@selector(parentWindowWillClose:)
                        name:NSWindowWillCloseNotification
                      object:[[self view] window]];
  [defaultCenter addObserver:self
                    selector:@selector(parentWindowDidResignMain:)
                        name:NSWindowDidResignMainNotification
                      object:[[self view] window]];
}

// When going fullscreen we can run into trouble.  Our view is removed
// from the non-fullscreen window before the non-fullscreen window
// loses key, so our parentDidResignKey: callback never gets called.
// In addition, a bookmark folder controller needs to be autoreleased
// (in case it's in the event chain when closed), but the release
// implicitly needs to happen while it's connected to the original
// (non-fullscreen) window to "unlock bar visibility".  Such a
// contract isn't honored when going fullscreen with the menu option
// (not with the keyboard shortcut).  We fake it as best we can here.
// We have a similar problem leaving fullscreen.
- (void)willEnterOrLeaveFullscreen:(NSNotification*)notification {
  if (folderController_) {
    [self childFolderWillClose:folderController_];
    [self closeFolderAndStopTrackingMenus];
  }
}

// NSNotificationCenter callback.
- (void)parentWindowWillClose:(NSNotification*)notification {
  [self closeFolderAndStopTrackingMenus];
}

// NSNotificationCenter callback.
- (void)parentWindowDidResignMain:(NSNotification*)notification {
  [self closeFolderAndStopTrackingMenus];
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

// We don't change a preference; we only change visibility. Preference changing
// (global state) is handled in |BrowserWindowCocoa::ToggleBookmarkBar()|. We
// simply update based on what we're told.
- (void)updateVisibility {
  [self showBookmarkBarWithAnimation:NO];
}

- (void)updateHiddenState {
  BOOL oldHidden = [[self view] isHidden];
  BOOL newHidden = ![self isVisible];
  if (oldHidden != newHidden)
    [[self view] setHidden:newHidden];
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
    if ([self isAnimatingBetweenState:BookmarkBar::HIDDEN
                             andState:BookmarkBar::SHOW] ||
        [self isAnimatingBetweenState:BookmarkBar::SHOW
                             andState:BookmarkBar::DETACHED])
      return 0;

    // If we ever need any other animation cases, code would go here.
  }

  return [self isInState:BookmarkBar::SHOW] ? kBookmarkBarOverlap : 0;
}

- (CGFloat)toolbarDividerOpacity {
  // Some special cases....
  if ([self isAnimationRunning]) {
    // In general, the toolbar shouldn't show a divider while we're animating
    // between showing and hidden. The exception is when our height is < 1, in
    // which case we can't draw it. It's all-or-nothing (no partial opacity).
    if ([self isAnimatingBetweenState:BookmarkBar::HIDDEN
                             andState:BookmarkBar::SHOW])
      return (NSHeight([[self view] frame]) < 1) ? 1 : 0;

    // The toolbar should show the divider when animating between showing and
    // detached (but opacity will vary).
    if ([self isAnimatingBetweenState:BookmarkBar::SHOW
                             andState:BookmarkBar::DETACHED])
      return static_cast<CGFloat>([self detachedMorphProgress]);

    // If we ever need any other animation cases, code would go here.
  }

  // In general, only show the divider when it's in the normal showing state.
  return [self isInState:BookmarkBar::SHOW] ? 0 : 1;
}

- (NSImage*)faviconForNode:(const BookmarkNode*)node {
  if (!node)
    return defaultImage_;

  if (node->is_folder())
    return folderImage_;

  const gfx::Image& favicon = bookmarkModel_->GetFavicon(node);
  if (!favicon.IsEmpty())
    return favicon.ToNSImage();

  return defaultImage_;
}

- (void)closeFolderAndStopTrackingMenus {
  showFolderMenus_ = NO;
  [self closeAllBookmarkFolders];
}

- (BOOL)canEditBookmarks {
  PrefService* prefs = browser_->profile()->GetPrefs();
  return prefs->GetBoolean(prefs::kEditBookmarksEnabled);
}

- (BOOL)canEditBookmark:(const BookmarkNode*)node {
  // Don't allow edit/delete of the permanent nodes.
  if (node == nil || bookmarkModel_->is_permanent_node(node))
    return NO;
  return YES;
}

#pragma mark Actions

// Helper methods called on the main thread by runMenuFlashThread.

- (void)setButtonFlashStateOn:(id)sender {
  [sender highlight:YES];
}

- (void)setButtonFlashStateOff:(id)sender {
  [sender highlight:NO];
}

-(void)cleanupAfterMenuFlashThread:(id)sender {
  [self closeFolderAndStopTrackingMenus];

  // Items retained by doMenuFlashOnSeparateThread below.
  [sender release];
  [self release];
}

// End runMenuFlashThread helper methods.

// This call is invoked only by doMenuFlashOnSeparateThread below.
// It makes the selected BookmarkButton (which is masquerading as a menu item)
// flash a few times to give confirmation feedback, then it closes the menu.
// It spends all its time sleeping or scheduling UI work on the main thread.
- (void)runMenuFlashThread:(id)sender {

  // Check this is not running on the main thread, as it sleeps.
  DCHECK(![NSThread isMainThread]);

  // Duration of flash phases and number of flashes designed to evoke a
  // slightly retro "more mac-like than the Mac" feel.
  // Current Cocoa UI has a barely perceptible flash,probably because Apple
  // doesn't fire the action til after the animation and so there's a hurry.
  // As this code is fully asynchronous, it can take its time.
  const float kBBOnFlashTime = 0.08;
  const float kBBOffFlashTime = 0.08;
  const int kBookmarkButtonMenuFlashes = 3;

  for (int count = 0 ; count < kBookmarkButtonMenuFlashes ; count++) {
    [self performSelectorOnMainThread:@selector(setButtonFlashStateOn:)
                           withObject:sender
                        waitUntilDone:NO];
    [NSThread sleepForTimeInterval:kBBOnFlashTime];
    [self performSelectorOnMainThread:@selector(setButtonFlashStateOff:)
                           withObject:sender
                        waitUntilDone:NO];
    [NSThread sleepForTimeInterval:kBBOffFlashTime];
  }
  [self performSelectorOnMainThread:@selector(cleanupAfterMenuFlashThread:)
                         withObject:sender
                      waitUntilDone:NO];
}

// Non-blocking call which starts the process to make the selected menu item
// flash a few times to give confirmation feedback, after which it closes the
// menu. The item is of course actually a BookmarkButton masquerading as a menu
// item).
- (void)doMenuFlashOnSeparateThread:(id)sender {

  // Ensure that self and sender don't go away before the animation completes.
  // These retains are balanced in cleanupAfterMenuFlashThread above.
  [self retain];
  [sender retain];
  [NSThread detachNewThreadSelector:@selector(runMenuFlashThread:)
                           toTarget:self
                         withObject:sender];
}

- (IBAction)openBookmark:(id)sender {
  BOOL isMenuItem = [[sender cell] isFolderButtonCell];
  BOOL animate = isMenuItem && innerContentAnimationsEnabled_;
  if (animate)
    [self doMenuFlashOnSeparateThread:sender];
  DCHECK([sender respondsToSelector:@selector(bookmarkNode)]);
  const BookmarkNode* node = [sender bookmarkNode];
  WindowOpenDisposition disposition =
      event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  RecordAppLaunch(browser_->profile(), node->url());
  [self openURL:node->url() disposition:disposition];

  if (!animate)
    [self closeFolderAndStopTrackingMenus];
}

// Common function to open a bookmark folder of any type.
- (void)openBookmarkFolder:(id)sender {
  DCHECK([sender isKindOfClass:[BookmarkButton class]]);
  DCHECK([[sender cell] isKindOfClass:[BookmarkButtonCell class]]);

  showFolderMenus_ = !showFolderMenus_;

  if (sender == offTheSideButton_)
    [[sender cell] setStartingChildIndex:displayedButtonCount_];

  // Toggle presentation of bar folder menus.
  [folderTarget_ openBookmarkFolderFromButton:sender];
}


// Click on a bookmark folder button.
- (IBAction)openBookmarkFolderFromButton:(id)sender {
  [self openBookmarkFolder:sender];
}

// Click on the "off the side" button (chevron), which opens like a folder
// button but isn't exactly a parent folder.
- (IBAction)openOffTheSideFolderFromButton:(id)sender {
  [self openBookmarkFolder:sender];
}

- (IBAction)openBookmarkInNewForegroundTab:(id)sender {
  const BookmarkNode* node = [self nodeFromMenuItem:sender];
  if (node)
    [self openURL:node->url() disposition:NEW_FOREGROUND_TAB];
  [self closeAllBookmarkFolders];
}

- (IBAction)openBookmarkInNewWindow:(id)sender {
  const BookmarkNode* node = [self nodeFromMenuItem:sender];
  if (node) {
    [self openURL:node->url() disposition:NEW_WINDOW];
    [self unhighlightBookmark:node];
  }
}

- (IBAction)openBookmarkInIncognitoWindow:(id)sender {
  const BookmarkNode* node = [self nodeFromMenuItem:sender];
  if (node) {
    [self openURL:node->url() disposition:OFF_THE_RECORD];
    [self unhighlightBookmark:node];
  }
}

- (IBAction)editBookmark:(id)sender {
  [self closeFolderAndStopTrackingMenus];
  const BookmarkNode* node = [self nodeFromMenuItem:sender];
  if (!node)
    return;

  [self unhighlightBookmark:node];

  if (node->is_folder()) {
    BookmarkNameFolderController* controller =
        [[BookmarkNameFolderController alloc]
          initWithParentWindow:[[self view] window]
                       profile:browser_->profile()
                          node:node];
    [controller runAsModalSheet];
    return;
  }

  // This jumps to a platform-common routine at this point (which may just
  // jump back to objc or may use the WebUI dialog).
  //
  // TODO(jrg): identify when we NO_TREE.  I can see it in the code
  // for the other platforms but can't find a way to trigger it in the
  // UI.
  BookmarkEditor::Show([[self view] window],
                       browser_->profile(),
                       BookmarkEditor::EditDetails::EditNode(node),
                       BookmarkEditor::SHOW_TREE);
}

- (IBAction)cutBookmark:(id)sender {
  const BookmarkNode* node = [self nodeFromMenuItem:sender];
  if (node) {
    std::vector<const BookmarkNode*> nodes;
    nodes.push_back(node);
    bookmark_utils::CopyToClipboard(bookmarkModel_, nodes, true);
  }
}

- (IBAction)copyBookmark:(id)sender {
  const BookmarkNode* node = [self nodeFromMenuItem:sender];
  if (node) {
    std::vector<const BookmarkNode*> nodes;
    nodes.push_back(node);
    bookmark_utils::CopyToClipboard(bookmarkModel_, nodes, false);
  }
}

// Paste the copied node immediately after the node for which the context
// menu has been presented if the node is a non-folder bookmark, otherwise
// past at the end of the folder node.
- (IBAction)pasteBookmark:(id)sender {
  const BookmarkNode* node = [self nodeFromMenuItem:sender];
  if (node) {
    int index = -1;
    if (node != bookmarkModel_->bookmark_bar_node() && !node->is_folder()) {
      const BookmarkNode* parent = node->parent();
      index = parent->GetIndexOf(node) + 1;
      if (index > parent->child_count())
        index = -1;
      node = parent;
    }
    bookmark_utils::PasteFromClipboard(bookmarkModel_, node, index);
  }
}

- (IBAction)deleteBookmark:(id)sender {
  const BookmarkNode* node = [self nodeFromMenuItem:sender];
  if (node) {
    bookmarkModel_->Remove(node->parent(),
                           node->parent()->GetIndexOf(node));
  }
}

- (IBAction)openAllBookmarks:(id)sender {
  const BookmarkNode* node = [self nodeFromMenuItem:sender];
  if (node) {
    [self openAll:node disposition:NEW_FOREGROUND_TAB];
    content::RecordAction(UserMetricsAction("OpenAllBookmarks"));
  }
}

- (IBAction)openAllBookmarksNewWindow:(id)sender {
  const BookmarkNode* node = [self nodeFromMenuItem:sender];
  if (node) {
    [self openAll:node disposition:NEW_WINDOW];
    content::RecordAction(UserMetricsAction("OpenAllBookmarksNewWindow"));
    [self unhighlightBookmark:node];
  }
}

- (IBAction)openAllBookmarksIncognitoWindow:(id)sender {
  const BookmarkNode* node = [self nodeFromMenuItem:sender];
  if (node) {
    [self openAll:node disposition:OFF_THE_RECORD];
    content::RecordAction(
        UserMetricsAction("OpenAllBookmarksIncognitoWindow"));

    [self unhighlightBookmark:node];
  }
}

// May be called from the bar or from a folder button.
// If called from a button, that button becomes the parent.
- (IBAction)addPage:(id)sender {
  const BookmarkNode* parent = [self nodeFromMenuItem:sender];
  if (!parent)
    parent = bookmarkModel_->bookmark_bar_node();
  GURL url;
  string16 title;
  chrome::GetURLAndTitleToBookmark(chrome::GetActiveWebContents(browser_),
                                   &url, &title);
  BookmarkEditor::Show([[self view] window],
                       browser_->profile(),
                       BookmarkEditor::EditDetails::AddNodeInFolder(
                           parent, -1, url, title),
                       BookmarkEditor::SHOW_TREE);

  [self unhighlightBookmark:parent];
}

// Might be called from the context menu over the bar OR over a
// button.  If called from a button, that button becomes a sibling of
// the new node.  If called from the bar, add to the end of the bar.
- (IBAction)addFolder:(id)sender {
  const BookmarkNode* senderNode = [self nodeFromMenuItem:sender];
  const BookmarkNode* parent = NULL;
  int newIndex = 0;
  // If triggered from the bar, folder or "others" folder - add as a child to
  // the end.
  // If triggered from a bookmark, add as next sibling.
  BookmarkNode::Type type = senderNode->type();
  if (type == BookmarkNode::BOOKMARK_BAR ||
      type == BookmarkNode::OTHER_NODE ||
      type == BookmarkNode::MOBILE ||
      type == BookmarkNode::FOLDER) {
    parent = senderNode;
    newIndex = parent->child_count();
  } else {
    parent = senderNode->parent();
    newIndex = parent->GetIndexOf(senderNode) + 1;
  }
  BookmarkNameFolderController* controller =
      [[BookmarkNameFolderController alloc]
        initWithParentWindow:[[self view] window]
                     profile:browser_->profile()
                      parent:parent
                    newIndex:newIndex];
  [controller runAsModalSheet];

  [self unhighlightBookmark:senderNode];
}

- (IBAction)importBookmarks:(id)sender {
  chrome::ShowImportDialog(browser_);
}

#pragma mark Private Methods

// Called after a theme change took place, possibly for a different profile.
- (void)themeDidChangeNotification:(NSNotification*)notification {
  [self updateTheme:[[[self view] window] themeProvider]];
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

// Position the off-the-side chevron to the left of the otherBookmarks button,
// unless it's hidden in which case it's right aligned on top of it.
- (void)positionOffTheSideButton {
  NSRect frame = [offTheSideButton_ frame];
  frame.size.height = bookmarks::kBookmarkFolderButtonHeight;
  if (otherBookmarksButton_.get() && ![otherBookmarksButton_ isHidden]) {
    frame.origin.x = ([otherBookmarksButton_ frame].origin.x -
                      (frame.size.width +
                       bookmarks::kBookmarkHorizontalPadding));
  } else {
    frame.origin.x = (NSMaxX([otherBookmarksButton_ frame]) - frame.size.width);
  }
  [offTheSideButton_ setFrame:frame];
}

// Configure the off-the-side button (e.g. specify the node range,
// check if we should enable or disable it, etc).
- (void)configureOffTheSideButtonContentsAndVisibility {
  // If deleting a button while off-the-side is open, buttons may be
  // promoted from off-the-side to the bar.  Accomodate.
  if (folderController_ &&
      ([folderController_ parentButton] == offTheSideButton_)) {
    [folderController_ reconfigureMenu];
  }

  [[offTheSideButton_ cell] setStartingChildIndex:displayedButtonCount_];
  [[offTheSideButton_ cell]
   setBookmarkNode:bookmarkModel_->bookmark_bar_node()];
  int bookmarkChildren = bookmarkModel_->bookmark_bar_node()->child_count();
  if (bookmarkChildren > displayedButtonCount_) {
    [offTheSideButton_ setHidden:NO];
  } else {
    // If we just deleted the last item in an off-the-side menu so the
    // button will be going away, make sure the menu goes away.
    if (folderController_ &&
        ([folderController_ parentButton] == offTheSideButton_))
      [self closeAllBookmarkFolders];
    // (And hide the button, too.)
    [offTheSideButton_ setHidden:YES];
  }
}

// Main menubar observation code, so we can know to close our fake menus if the
// user clicks on the actual menubar, as multiple unconnected menus sharing
// the screen looks weird.
// Needed because the local event monitor doesn't see the click on the menubar.

// Gets called when the menubar is clicked.
- (void)begunTracking:(NSNotification *)notification {
  [self closeFolderAndStopTrackingMenus];
}

// Install the callback.
- (void)startObservingMenubar {
  NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
  [nc addObserver:self
         selector:@selector(begunTracking:)
             name:NSMenuDidBeginTrackingNotification
           object:[NSApp mainMenu]];
}

// Remove the callback.
- (void)stopObservingMenubar {
  NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
  [nc removeObserver:self
                name:NSMenuDidBeginTrackingNotification
              object:[NSApp mainMenu]];
}

// End of menubar observation code.

// Begin (or end) watching for a click outside this window.  Unlike
// normal NSWindows, bookmark folder "fake menu" windows do not become
// key or main.  Thus, traditional notification (e.g. WillResignKey)
// won't work.  Our strategy is to watch (at the app level) for a
// "click outside" these windows to detect when they logically lose
// focus.
- (void)watchForExitEvent:(BOOL)watch {
  if (watch) {
    if (!exitEventTap_) {
      exitEventTap_ = [NSEvent
          addLocalMonitorForEventsMatchingMask:NSAnyEventMask
          handler:^NSEvent* (NSEvent* event) {
              if ([self isEventAnExitEvent:event])
                [self closeFolderAndStopTrackingMenus];
              return event;
          }];
      [self startObservingMenubar];
    }
  } else {
    if (exitEventTap_) {
      [NSEvent removeMonitor:exitEventTap_];
      exitEventTap_ = nil;
      [self stopObservingMenubar];
    }
  }
}

// Keep the "no items" label centered in response to a frame size change.
- (void)centerNoItemsLabel {
  // Note that this computation is done in the parent's coordinate system,
  // which is unflipped. Also, we want the label to be a fixed distance from
  // the bottom, so that it slides up properly (on animating to hidden).
  // The textfield sits in the itemcontainer, so to center it we maintain
  // equal vertical padding on the top and bottom.
  int yoffset = (NSHeight([[buttonView_ noItemTextfield] frame]) -
                 NSHeight([[buttonView_ noItemContainer] frame])) / 2;
  [[buttonView_ noItemContainer] setFrameOrigin:NSMakePoint(0, yoffset)];
}

// (Private)
- (void)showBookmarkBarWithAnimation:(BOOL)animate {
  if (animate && stateAnimationsEnabled_) {
    // If |-doBookmarkBarAnimation| does the animation, we're done.
    if ([self doBookmarkBarAnimation])
      return;

    // Else fall through and do the change instantly.
  }

  // Set our height.
  [resizeDelegate_ resizeView:[self view]
                    newHeight:[self preferredHeight]];

  // Only show the divider if showing the normal bookmark bar.
  BOOL showsDivider = [self isInState:BookmarkBar::SHOW];
  [[self backgroundGradientView] setShowsDivider:showsDivider];

  // Make sure we're shown.
  [[self view] setHidden:![self isVisible]];

  // Update everything else.
  [self layoutSubviews];
  [self frameDidChange];
  [self updateNoItemContainerVisibility];
}

// (Private)
- (BOOL)doBookmarkBarAnimation {
  if ([self isAnimatingFromState:BookmarkBar::HIDDEN
                         toState:BookmarkBar::SHOW]) {
    [[self backgroundGradientView] setShowsDivider:YES];
    [[self view] setHidden:NO];
    AnimatableView* view = [self animatableView];
    // Height takes into account the extra height we have since the toolbar
    // only compresses when we're done.
    [view animateToNewHeight:(bookmarks::kBookmarkBarHeight -
                              kBookmarkBarOverlap)
                    duration:kBookmarkBarAnimationDuration];
  } else if ([self isAnimatingFromState:BookmarkBar::SHOW
                                toState:BookmarkBar::HIDDEN]) {
    [[self backgroundGradientView] setShowsDivider:YES];
    [[self view] setHidden:NO];
    AnimatableView* view = [self animatableView];
    [view animateToNewHeight:0
                    duration:kBookmarkBarAnimationDuration];
  } else if ([self isAnimatingFromState:BookmarkBar::SHOW
                                toState:BookmarkBar::DETACHED]) {
    [[self backgroundGradientView] setShowsDivider:YES];
    [[self view] setHidden:NO];
    AnimatableView* view = [self animatableView];
    [view animateToNewHeight:chrome::kNTPBookmarkBarHeight
                    duration:kBookmarkBarAnimationDuration];
  } else if ([self isAnimatingFromState:BookmarkBar::DETACHED
                                toState:BookmarkBar::SHOW]) {
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

// Enable or disable items.  We are the menu delegate for both the bar
// and for bookmark folder buttons.
- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)anItem {
  // NSUserInterfaceValidations says that the passed-in object has type
  // |id<NSValidatedUserInterfaceItem>|, but this function needs to call the
  // NSObject method -isKindOfClass: on the parameter. In theory, this is not
  // correct, but this is probably a bug in the method signature.
  NSMenuItem* item = static_cast<NSMenuItem*>(anItem);
  // Yes for everything we don't explicitly deny.
  if (![item isKindOfClass:[NSMenuItem class]])
    return YES;

  // Yes if we're not a special BookmarkMenu.
  if (![[item menu] isKindOfClass:[BookmarkMenu class]])
    return YES;

  // No if we think it's a special BookmarkMenu but have trouble.
  const BookmarkNode* node = [self nodeFromMenuItem:item];
  if (!node)
    return NO;

  // If this is the bar menu, we only have things to do if there are
  // buttons.  If this is a folder button menu, we only have things to
  // do if the folder has items.
  NSMenu* menu = [item menu];
  BOOL thingsToDo = NO;
  if (menu == [[self view] menu]) {
    thingsToDo = [buttons_ count] ? YES : NO;
  } else {
    if (node && node->is_folder() && !node->empty()) {
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

  bool can_edit = [self canEditBookmarks];
  if ((action == @selector(editBookmark:)) ||
      (action == @selector(deleteBookmark:)) ||
      (action == @selector(cutBookmark:)) ||
      (action == @selector(copyBookmark:))) {
    if (![self canEditBookmark:node])
      return NO;
    if (action != @selector(copyBookmark:) && !can_edit)
      return NO;
  }

  if (action == @selector(pasteBookmark:) &&
      (!bookmark_utils::CanPasteFromClipboard(node) || !can_edit)) {
      return NO;
  }

  if ((!can_edit) &&
      ((action == @selector(addPage:)) ||
       (action == @selector(addFolder:)))) {
    return NO;
  }

  Profile* profile = browser_->profile();
  // If this is an incognito window, don't allow "open in incognito".
  if ((action == @selector(openBookmarkInIncognitoWindow:)) ||
      (action == @selector(openAllBookmarksIncognitoWindow:))) {
    if (profile->IsOffTheRecord() ||
        IncognitoModePrefs::GetAvailability(profile->GetPrefs()) ==
            IncognitoModePrefs::DISABLED) {
      return NO;
    }
  }

  // If Incognito mode is forced, do not let open bookmarks in normal window.
  if ((action == @selector(openBookmark:)) ||
      (action == @selector(openAllBookmarksNewWindow:)) ||
      (action == @selector(openAllBookmarks:))) {
    if (IncognitoModePrefs::GetAvailability(profile->GetPrefs()) ==
            IncognitoModePrefs::FORCED) {
      return NO;
    }
  }

  // Enabled by default.
  return YES;
}

// Actually open the URL.  This is the last chance for a unit test to
// override.
- (void)openURL:(GURL)url disposition:(WindowOpenDisposition)disposition {
  OpenURLParams params(
      url, Referrer(), disposition, content::PAGE_TRANSITION_AUTO_BOOKMARK,
      false);
  browser_->OpenURL(params);
}

- (void)clearMenuTagMap {
  seedId_ = 0;
  menuTagMap_.clear();
}

- (int)preferredHeight {
  DCHECK(![self isAnimationRunning]);

  if (!barIsEnabled_)
    return 0;

  switch (state_) {
    case BookmarkBar::SHOW:
      return bookmarks::kBookmarkBarHeight;
    case BookmarkBar::DETACHED:
      return chrome::kNTPBookmarkBarHeight;
    case BookmarkBar::HIDDEN:
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
  [item setImage:[self faviconForNode:child]];
  if (child->is_folder()) {
    NSMenu* submenu = [[[NSMenu alloc] initWithTitle:title] autorelease];
    [menu setSubmenu:submenu forItem:item];
    if (!child->empty()) {
      [self addFolderNode:child toMenu:submenu];  // potentially recursive
    } else {
      [self tagEmptyMenu:submenu];
    }
  } else {
    [item setTarget:self];
    [item setAction:@selector(openBookmarkMenuItem:)];
    [item setTag:[self menuTagFromNodeId:child->id()]];
    if (child->is_url())
      [item setToolTip:[BookmarkMenuCocoaController tooltipForNode:child]];
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
  for (int i = 0; i < node->child_count(); i++) {
    const BookmarkNode* child = node->GetChild(i);
    [self addNode:child toMenu:menu];
  }
}

// Return an autoreleased NSMenu that represents the given bookmark
// folder node.
- (NSMenu *)menuForFolderNode:(const BookmarkNode*)node {
  if (!node->is_folder())
    return nil;
  NSString* title = base::SysUTF16ToNSString(node->GetTitle());
  NSMenu* menu = [[[NSMenu alloc] initWithTitle:title] autorelease];
  [self addFolderNode:node toMenu:menu];

  if (![menu numberOfItems]) {
    [self tagEmptyMenu:menu];
  }
  return menu;
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

- (IBAction)openBookmarkMenuItem:(id)sender {
  int64 tag = [self nodeIdFromMenuTag:[sender tag]];
  const BookmarkNode* node = bookmarkModel_->GetNodeByID(tag);
  WindowOpenDisposition disposition =
      event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  [self openURL:node->url() disposition:disposition];
}

// For the given root node of the bookmark bar, show or hide (as
// appropriate) the "no items" container (text which says "bookmarks
// go here").
- (void)showOrHideNoItemContainerForNode:(const BookmarkNode*)node {
  isEmpty_ = node->empty();
  [[self view] setNeedsDisplay:YES];
  [self updateNoItemContainerVisibility];
}

- (void)updateNoItemContainerVisibility {
  BOOL hideNoItemWarning = !isEmpty_ ||
      ([self shouldShowAtBottomWhenDetached] &&
       state_ == BookmarkBar::DETACHED);
  [[buttonView_ noItemContainer] setHidden:hideNoItemWarning];
}

// TODO(jrg): write a "build bar" so there is a nice spot for things
// like the contextual menu which is invoked when not over a
// bookmark.  On Safari that menu has a "new folder" option.
- (void)addNodesToButtonList:(const BookmarkNode*)node {
  [self showOrHideNoItemContainerForNode:node];

  CGFloat maxViewX = NSMaxX([[self view] bounds]);
  int xOffset = 0;
  for (int i = 0; i < node->child_count(); i++) {
    const BookmarkNode* child = node->GetChild(i);
    BookmarkButton* button = [self buttonForNode:child xOffset:&xOffset];
    if (NSMinX([button frame]) >= maxViewX) {
      [button setDelegate:nil];
      break;
    }
    [buttons_ addObject:button];
  }
}

- (BookmarkButton*)buttonForNode:(const BookmarkNode*)node
                         xOffset:(int*)xOffset {
  BookmarkButtonCell* cell = [self cellForBookmarkNode:node];
  NSRect frame = [self frameForBookmarkButtonFromCell:cell xOffset:xOffset];

  scoped_nsobject<BookmarkButton>
      button([[BookmarkButton alloc] initWithFrame:frame]);
  DCHECK(button.get());

  // [NSButton setCell:] warns to NOT use setCell: other than in the
  // initializer of a control.  However, we are using a basic
  // NSButton whose initializer does not take an NSCell as an
  // object.  To honor the assumed semantics, we do nothing with
  // NSButton between alloc/init and setCell:.
  [button setCell:cell];
  [button setDelegate:self];

  // We cannot set the button cell's text color until it is placed in
  // the button (e.g. the [button setCell:cell] call right above).  We
  // also cannot set the cell's text color until the view is added to
  // the hierarchy.  If that second part is now true, set the color.
  // (If not we'll set the color on the 1st themeChanged:
  // notification.)
  ui::ThemeProvider* themeProvider = [[[self view] window] themeProvider];
  if (themeProvider) {
    NSColor* color =
        themeProvider->GetNSColor(ThemeService::COLOR_BOOKMARK_TEXT,
                                  true);
    [cell setTextColor:color];
  }

  if (node->is_folder()) {
    [button setTarget:self];
    [button setAction:@selector(openBookmarkFolderFromButton:)];
    [[button draggableButton] setActsOnMouseDown:YES];
    // If it has a title, and it will be truncated, show full title in
    // tooltip.
    NSString* title = base::SysUTF16ToNSString(node->GetTitle());
    if ([title length] &&
        [[button cell] cellSize].width > bookmarks::kDefaultBookmarkWidth) {
      [button setToolTip:title];
    }
  } else {
    // Make the button do something
    [button setTarget:self];
    [button setAction:@selector(openBookmark:)];
    if (node->is_url())
      [button setToolTip:[BookmarkMenuCocoaController tooltipForNode:node]];
  }
  return [[button.get() retain] autorelease];
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
// which are clipped.  Called when building the bookmark bar the first time.
- (void)addButtonsToView {
  displayedButtonCount_ = 0;
  NSMutableArray* buttons = [self buttons];
  for (NSButton* button in buttons) {
    if (NSMaxX([button frame]) > (NSMinX([offTheSideButton_ frame]) -
                                  bookmarks::kBookmarkHorizontalPadding))
      break;
    [buttonView_ addSubview:button];
    ++displayedButtonCount_;
  }
  NSUInteger removalCount =
      [buttons count] - (NSUInteger)displayedButtonCount_;
  if (removalCount > 0) {
    NSRange removalRange = NSMakeRange(displayedButtonCount_, removalCount);
    [buttons removeObjectsInRange:removalRange];
  }
}

// Shows or hides the Other Bookmarks button as appropriate, and returns
// whether it ended up visible.
- (BOOL)setOtherBookmarksButtonVisibility {
  if (!otherBookmarksButton_.get())
    return NO;

  BOOL visible = ![otherBookmarksButton_ bookmarkNode]->empty();
  [otherBookmarksButton_ setHidden:!visible];
  return visible;
}

// Create the button for "Other Bookmarks" on the right of the bar.
- (void)createOtherBookmarksButton {
  // Can't create this until the model is loaded, but only need to
  // create it once.
  if (otherBookmarksButton_.get()) {
    [self setOtherBookmarksButtonVisibility];
    return;
  }

  // TODO(jrg): remove duplicate code
  NSCell* cell = [self cellForBookmarkNode:bookmarkModel_->other_node()];
  int ignored = 0;
  NSRect frame = [self frameForBookmarkButtonFromCell:cell xOffset:&ignored];
  frame.origin.x = [[self buttonView] bounds].size.width - frame.size.width;
  frame.origin.x -= bookmarks::kBookmarkHorizontalPadding;
  BookmarkButton* button = [[BookmarkButton alloc] initWithFrame:frame];
  [[button draggableButton] setDraggable:NO];
  [[button draggableButton] setActsOnMouseDown:YES];
  otherBookmarksButton_.reset(button);
  view_id_util::SetID(button, VIEW_ID_OTHER_BOOKMARKS);

  // Make sure this button, like all other BookmarkButtons, lives
  // until the end of the current event loop.
  [[button retain] autorelease];

  // Peg at right; keep same height as bar.
  [button setAutoresizingMask:(NSViewMinXMargin)];
  [button setCell:cell];
  [button setDelegate:self];
  [button setTarget:self];
  [button setAction:@selector(openBookmarkFolderFromButton:)];
  [buttonView_ addSubview:button];

  [self setOtherBookmarksButtonVisibility];

  // Now that it's here, move the chevron over.
  [self positionOffTheSideButton];
}

// Now that the model is loaded, set the bookmark bar root as the node
// represented by the bookmark bar (default, background) menu.
- (void)setNodeForBarMenu {
  const BookmarkNode* node = bookmarkModel_->bookmark_bar_node();
  BookmarkMenu* menu = static_cast<BookmarkMenu*>([[self view] menu]);

  // Make sure types are compatible
  DCHECK(sizeof(long long) == sizeof(int64));
  [menu setRepresentedObject:[NSNumber numberWithLongLong:node->id()]];
}

// To avoid problems with sync, changes that may impact the current
// bookmark (e.g. deletion) make sure context menus are closed.  This
// prevents deleting a node which no longer exists.
- (void)cancelMenuTracking {
  [buttonContextMenu_ cancelTracking];
  [buttonFolderContextMenu_ cancelTracking];
}

- (void)moveToState:(BookmarkBar::State)nextState
      withAnimation:(BOOL)animate {
  BOOL isAnimationRunning = [self isAnimationRunning];

  // No-op if the next state is the same as the "current" one, subject to the
  // following conditions:
  //  - no animation is running; or
  //  - an animation is running and |animate| is YES ([*] if it's NO, we'd want
  //    to cancel the animation and jump to the final state).
  if ((nextState == state_) && (!isAnimationRunning || animate))
    return;

  // If an animation is running, we want to finalize it. Otherwise we'd have to
  // be able to animate starting from the middle of one type of animation. We
  // assume that animations that we know about can be "reversed".
  if (isAnimationRunning) {
    // Don't cancel if we're going to reverse the animation.
    if (nextState != lastState_) {
      [self stopCurrentAnimation];
      [self finalizeState];
    }

    // If we're in case [*] above, we can stop here.
    if (nextState == state_)
      return;
  }

  // Now update with the new state change.
  lastState_ = state_;
  state_ = nextState;
  isAnimationRunning_ = YES;

  // Animate only if told to and if bar is enabled.
  if (animate && stateAnimationsEnabled_ && barIsEnabled_) {
    [self closeAllBookmarkFolders];
    // Take care of any animation cases we know how to handle.

    // We know how to handle hidden <-> normal, normal <-> detached....
    if ([self isAnimatingBetweenState:BookmarkBar::HIDDEN
                             andState:BookmarkBar::SHOW] ||
        [self isAnimatingBetweenState:BookmarkBar::SHOW
                             andState:BookmarkBar::DETACHED]) {
      [delegate_ bookmarkBar:self
        willAnimateFromState:lastState_
                     toState:state_];
      [self showBookmarkBarWithAnimation:YES];
      return;
    }

    // If we ever need any other animation cases, code would go here.
    // Let any animation cases which we don't know how to handle fall through to
    // the unanimated case.
  }

  // Just jump to the state.
  [self finalizeState];
}

// N.B.: |-moveToState:...| will check if this should be a no-op or not.
- (void)updateState:(BookmarkBar::State)newState
         changeType:(BookmarkBar::AnimateChangeType)changeType {
  BOOL animate = changeType == BookmarkBar::ANIMATE_STATE_CHANGE &&
                 stateAnimationsEnabled_;
  [self moveToState:newState withAnimation:animate];
}

// (Private)
- (void)finalizeState {
  // We promise that our delegate that the variables will be finalized before
  // the call to |-bookmarkBar:didChangeFromState:toState:|.
  BookmarkBar::State oldState = lastState_;
  lastState_ = state_;
  isAnimationRunning_ = NO;

  // Notify our delegate.
  [delegate_ bookmarkBar:self
      didChangeFromState:oldState
                 toState:state_];

  // Update ourselves visually.
  [self updateVisibility];
}

// (Private)
- (void)stopCurrentAnimation {
  [[self animatableView] stopAnimation];
}

// Delegate method for |AnimatableView| (a superclass of
// |BookmarkBarToolbarView|).
- (void)animationDidEnd:(NSAnimation*)animation {
  [self finalizeState];
}

- (void)reconfigureBookmarkBar {
  [self redistributeButtonsOnBarAsNeeded];
  [self positionOffTheSideButton];
  [self configureOffTheSideButtonContentsAndVisibility];
  [self centerNoItemsLabel];
}

// Determine if the given |view| can completely fit within the constraint of
// maximum x, given by |maxViewX|, and, if not, narrow the view up to a minimum
// width. If the minimum width is not achievable then hide the view. Return YES
// if the view was hidden.
- (BOOL)shrinkOrHideView:(NSView*)view forMaxX:(CGFloat)maxViewX {
  BOOL wasHidden = NO;
  // See if the view needs to be narrowed.
  NSRect frame = [view frame];
  if (NSMaxX(frame) > maxViewX) {
    // Resize if more than 30 pixels are showing, otherwise hide.
    if (NSMinX(frame) + 30.0 < maxViewX) {
      frame.size.width = maxViewX - NSMinX(frame);
      [view setFrame:frame];
    } else {
      [view setHidden:YES];
      wasHidden = YES;
    }
  }
  return wasHidden;
}

// Bookmark button menu items that open a new window (e.g., open in new window,
// open in incognito, edit, etc.) cause us to lose a mouse-exited event
// on the button, which leaves it in a hover state.
// Since the showsBorderOnlyWhileMouseInside uses a tracking area, simple
// tricks (e.g. sending an extra mouseExited: to the button) don't
// fix the problem.
// http://crbug.com/129338
-(void)unhighlightBookmark:(const BookmarkNode*)node {
  // Only relevant if context menu was opened from a button on the
  // bookmark bar.
  const BookmarkNode* parent = node->parent();
  BookmarkNode::Type parentType = parent->type();
  if (parentType == BookmarkNode::BOOKMARK_BAR) {
    int index = parent->GetIndexOf(node);
    if ((index >= 0) && (static_cast<NSUInteger>(index) < [buttons_ count])) {
      NSButton* button =
          [buttons_ objectAtIndex:static_cast<NSUInteger>(index)];
      if ([button showsBorderOnlyWhileMouseInside]) {
        [button setShowsBorderOnlyWhileMouseInside:NO];
        [button setShowsBorderOnlyWhileMouseInside:YES];
      }
    }
  }
}


// Adjust the horizontal width and the visibility of the "For quick access"
// text field and "Import bookmarks..." button based on the current width
// of the containing |buttonView_| (which is affected by window width).
- (void)adjustNoItemContainerWidthsForMaxX:(CGFloat)maxViewX {
  if (![[buttonView_ noItemContainer] isHidden]) {
    // Reset initial frames for the two items, then adjust as necessary.
    NSTextField* noItemTextfield = [buttonView_ noItemTextfield];
    [noItemTextfield setFrame:originalNoItemsRect_];
    [noItemTextfield setHidden:NO];
    NSButton* importBookmarksButton = [buttonView_ importBookmarksButton];
    [importBookmarksButton setFrame:originalImportBookmarksRect_];
    [importBookmarksButton setHidden:NO];
    // Check each to see if they need to be shrunk or hidden.
    if ([self shrinkOrHideView:importBookmarksButton forMaxX:maxViewX])
      [self shrinkOrHideView:noItemTextfield forMaxX:maxViewX];
  }
}

// Scans through all buttons from left to right, calculating from scratch where
// they should be based on the preceding widths, until it finds the one
// requested.
// Returns NSZeroRect if there is no such button in the bookmark bar.
// Enables you to work out where a button will end up when it is done animating.
- (NSRect)finalRectOfButton:(BookmarkButton*)wantedButton {
  CGFloat left = bookmarks::kBookmarkHorizontalPadding;
  NSRect buttonFrame = NSZeroRect;

  for (NSButton* button in buttons_.get()) {
    // Hidden buttons get no space.
    if ([button isHidden])
      continue;
    buttonFrame = [button frame];
    buttonFrame.origin.x = left;
    left += buttonFrame.size.width + bookmarks::kBookmarkHorizontalPadding;
    if (button == wantedButton)
      return buttonFrame;
  }
  return NSZeroRect;
}

// Calculates the final position of the last button in the bar.
// We can't just use [[self buttons] lastObject] frame] because the button
// may be animating currently.
- (NSRect)finalRectOfLastButton {
  return [self finalRectOfButton:[[self buttons] lastObject]];
}

- (void)redistributeButtonsOnBarAsNeeded {
  const BookmarkNode* node = bookmarkModel_->bookmark_bar_node();
  NSInteger barCount = node->child_count();

  // Determine the current maximum extent of the visible buttons.
  CGFloat maxViewX = NSMaxX([[self view] bounds]);
  NSButton* otherBookmarksButton = otherBookmarksButton_.get();
  // If necessary, pull in the width to account for the Other Bookmarks button.
  if ([self setOtherBookmarksButtonVisibility])
    maxViewX = [otherBookmarksButton frame].origin.x -
        bookmarks::kBookmarkHorizontalPadding;

  [self positionOffTheSideButton];
  // If we're already overflowing, then we need to account for the chevron.
  if (barCount > displayedButtonCount_)
    maxViewX = [offTheSideButton_ frame].origin.x -
        bookmarks::kBookmarkHorizontalPadding;

  // As a result of pasting or dragging, the bar may now have more buttons
  // than will fit so remove any which overflow.  They will be shown in
  // the off-the-side folder.
  while (displayedButtonCount_ > 0) {
    BookmarkButton* button = [buttons_ lastObject];
    if (NSMaxX([self finalRectOfLastButton]) < maxViewX)
      break;
    [buttons_ removeLastObject];
    [button setDelegate:nil];
    [button removeFromSuperview];
    --displayedButtonCount_;
  }

  // As a result of cutting, deleting and dragging, the bar may now have room
  // for more buttons.
  int xOffset = displayedButtonCount_ > 0 ?
      NSMaxX([self finalRectOfLastButton]) +
          bookmarks::kBookmarkHorizontalPadding : 0;
  for (int i = displayedButtonCount_; i < barCount; ++i) {
    const BookmarkNode* child = node->GetChild(i);
    BookmarkButton* button = [self buttonForNode:child xOffset:&xOffset];
    // If we're testing against the last possible button then account
    // for the chevron no longer needing to be shown.
    if (i == barCount + 1)
      maxViewX += NSWidth([offTheSideButton_ frame]) +
          bookmarks::kBookmarkHorizontalPadding;
    if (NSMaxX([button frame]) >= maxViewX) {
      [button setDelegate:nil];
      break;
    }
    ++displayedButtonCount_;
    [buttons_ addObject:button];
    [buttonView_ addSubview:button];
  }

  // While we're here, adjust the horizontal width and the visibility
  // of the "For quick access" and "Import bookmarks..." text fields.
  if (![buttons_ count])
    [self adjustNoItemContainerWidthsForMaxX:maxViewX];
}

#pragma mark Private Methods Exposed for Testing

- (BookmarkBarView*)buttonView {
  return buttonView_;
}

- (NSMutableArray*)buttons {
  return buttons_.get();
}

- (NSButton*)offTheSideButton {
  return offTheSideButton_;
}

- (BOOL)offTheSideButtonIsHidden {
  return [offTheSideButton_ isHidden];
}

- (BookmarkButton*)otherBookmarksButton {
  return otherBookmarksButton_.get();
}

- (BookmarkBarFolderController*)folderController {
  return folderController_;
}

- (id)folderTarget {
  return folderTarget_.get();
}

- (int)displayedButtonCount {
  return displayedButtonCount_;
}

// Delete all buttons (bookmarks, chevron, "other bookmarks") from the
// bookmark bar; reset knowledge of bookmarks.
- (void)clearBookmarkBar {
  for (BookmarkButton* button in buttons_.get()) {
    [button setDelegate:nil];
    [button removeFromSuperview];
  }
  [buttons_ removeAllObjects];
  [self clearMenuTagMap];
  displayedButtonCount_ = 0;

  // Make sure there are no stale pointers in the pasteboard.  This
  // can be important if a bookmark is deleted (via bookmark sync)
  // while in the middle of a drag.  The "drag completed" code
  // (e.g. [BookmarkBarView performDragOperationForBookmarkButton:]) is
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
- (BookmarkButtonCell*)cellForBookmarkNode:(const BookmarkNode*)node {
  NSImage* image = node ? [self faviconForNode:node] : nil;
  NSMenu* menu = node && node->is_folder() ? buttonFolderContextMenu_ :
      buttonContextMenu_;
  BookmarkButtonCell* cell = [BookmarkButtonCell buttonCellForNode:node
                                                       contextMenu:menu
                                                          cellText:nil
                                                         cellImage:image];
  [cell setTag:kStandardButtonTypeWithLimitedClickFeedback];

  // Note: a quirk of setting a cell's text color is that it won't work
  // until the cell is associated with a button, so we can't theme the cell yet.

  return cell;
}

// Returns a frame appropriate for the given bookmark cell, suitable
// for creating an NSButton that will contain it.  |xOffset| is the X
// offset for the frame; it is increased to be an appropriate X offset
// for the next button.
- (NSRect)frameForBookmarkButtonFromCell:(NSCell*)cell
                                 xOffset:(int*)xOffset {
  DCHECK(xOffset);
  NSRect bounds = [buttonView_ bounds];
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
- (void)checkForBookmarkButtonGrowth:(NSButton*)changedButton {
  NSRect frame = [changedButton frame];
  CGFloat desiredSize = [self widthForBookmarkButtonCell:[changedButton cell]];
  CGFloat delta = desiredSize - frame.size.width;
  if (delta) {
    frame.size.width = desiredSize;
    [changedButton setFrame:frame];
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
  [self configureOffTheSideButtonContentsAndVisibility];
}

// Called when our controlled frame has changed size.
- (void)frameDidChange {
  if (!bookmarkModel_->IsLoaded())
    return;
  [self updateTheme:[[[self view] window] themeProvider]];
  [self reconfigureBookmarkBar];
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

// Return the BookmarkNode associated with the given NSMenuItem.  Can
// return NULL which means "do nothing".  One case where it would
// return NULL is if the bookmark model gets modified while you have a
// context menu open.
- (const BookmarkNode*)nodeFromMenuItem:(id)sender {
  const BookmarkNode* node = NULL;
  BookmarkMenu* menu = (BookmarkMenu*)[sender menu];
  if ([menu isKindOfClass:[BookmarkMenu class]]) {
    int64 id = [menu id];
    node = bookmarkModel_->GetNodeByID(id);
  }
  return node;
}

// Adapt appearance of buttons to the current theme. Called after
// theme changes, or when our view is added to the view hierarchy.
// Oddly, the view pings us instead of us pinging our view.  This is
// because our trigger is an [NSView viewWillMoveToWindow:], which the
// controller doesn't normally know about.  Otherwise we don't have
// access to the theme before we know what window we will be on.
- (void)updateTheme:(ui::ThemeProvider*)themeProvider {
  if (!themeProvider)
    return;
  NSColor* color =
      themeProvider->GetNSColor(ThemeService::COLOR_BOOKMARK_TEXT,
                                true);
  for (BookmarkButton* button in buttons_.get()) {
    BookmarkButtonCell* cell = [button cell];
    [cell setTextColor:color];
  }
  [[otherBookmarksButton_ cell] setTextColor:color];
}

// Return YES if the event indicates an exit from the bookmark bar
// folder menus.  E.g. "click outside" of the area we are watching.
// At this time we are watching the area that includes all popup
// bookmark folder windows.
- (BOOL)isEventAnExitEvent:(NSEvent*)event {
  NSWindow* eventWindow = [event window];
  NSWindow* myWindow = [[self view] window];
  switch ([event type]) {
    case NSLeftMouseDown:
    case NSRightMouseDown:
      // If the click is in my window but NOT in the bookmark bar, consider
      // it a click 'outside'. Clicks directly on an active button (i.e. one
      // that is a folder and for which its folder menu is showing) are 'in'.
      // All other clicks on the bookmarks bar are counted as 'outside'
      // because they should close any open bookmark folder menu.
      if (eventWindow == myWindow) {
        NSView* hitView =
            [[eventWindow contentView] hitTest:[event locationInWindow]];
        if (hitView == [folderController_ parentButton])
          return NO;
        if (![hitView isDescendantOf:[self view]] || hitView == buttonView_)
          return YES;
      }
      // If a click in a bookmark bar folder window and that isn't
      // one of my bookmark bar folders, YES is click outside.
      if (![eventWindow isKindOfClass:[BookmarkBarFolderWindow
                                       class]]) {
        return YES;
      }
      break;
    case NSKeyDown: {
      // Event hooks often see the same keydown event twice due to the way key
      // events get dispatched and redispatched, so ignore if this keydown
      // event has the EXACT same timestamp as the previous keydown.
      static NSTimeInterval lastKeyDownEventTime;
      NSTimeInterval thisTime = [event timestamp];
      if (lastKeyDownEventTime != thisTime) {
        lastKeyDownEventTime = thisTime;
        if ([event modifierFlags] & NSCommandKeyMask)
          return YES;
        else if (folderController_)
          return [folderController_ handleInputText:[event characters]];
      }
      return NO;
    }
    case NSKeyUp:
      return NO;
    case NSLeftMouseDragged:
      // We can get here with the following sequence:
      // - open a bookmark folder
      // - right-click (and unclick) on it to open context menu
      // - move mouse to window titlebar then click-drag it by the titlebar
      // http://crbug.com/49333
      return NO;
    default:
      break;
  }
  return NO;
}

#pragma mark Drag & Drop

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
    // Hidden buttons can overlap valid visible buttons, just ignore.
    if ([button isHidden])
      continue;
    // Break early if we've gone too far.
    if ((NSMinX([button frame]) > point.x) || (![button superview]))
      return nil;
    // Careful -- this only applies to the bar with horiz buttons.
    // Intentionally NOT using NSPointInRect() so that scrolling into
    // a submenu doesn't cause it to be closed.
    if (ValueInRangeInclusive(NSMinX([button frame]),
                              point.x,
                              NSMaxX([button frame]))) {
      // Over a button but let's be a little more specific (make sure
      // it's over the middle half, not just over it).
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
  point = [[self view] convertPoint:point
                           fromView:[[[self view] window] contentView]];

  // If there's a hover button, return it if the point is within its bounds.
  // Since the logic in -buttonForDroppingOnAtPoint:fromArray: only matches a
  // button when the point is over the middle half, this is needed to prevent
  // the button's folder being closed if the mouse temporarily leaves the
  // middle half but is still within the button bounds.
  if (hoverButton_ && NSPointInRect(point, [hoverButton_ frame]))
     return hoverButton_.get();

  BookmarkButton* button = [self buttonForDroppingOnAtPoint:point
                                                  fromArray:buttons_.get()];
  // One more chance -- try "Other Bookmarks" and "off the side" (if visible).
  // This is different than BookmarkBarFolderController.
  if (!button) {
    NSMutableArray* array = [NSMutableArray array];
    if (![self offTheSideButtonIsHidden])
      [array addObject:offTheSideButton_];
    [array addObject:otherBookmarksButton_];
    button = [self buttonForDroppingOnAtPoint:point
                                    fromArray:array];
  }
  return button;
}

- (int)indexForDragToPoint:(NSPoint)point {
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
    int index = afterNode->parent()->GetIndexOf(afterNode);
    // Make sure we don't get confused by buttons which aren't visible.
    return std::min(index, displayedButtonCount_);
  }

  // If nothing is to my right I am at the end!
  return displayedButtonCount_;
}

// TODO(mrossetti,jrg): Yet more duplicated code.
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
    destIndex = [button bookmarkNode]->child_count();
  } else {
    // Else we're dropping somewhere on the bar, so find the right spot.
    destParent = bookmarkModel_->bookmark_bar_node();
    destIndex = [self indexForDragToPoint:point];
  }

  // Be sure we don't try and drop a folder into itself.
  if (sourceNode != destParent) {
    if (copy)
      bookmarkModel_->Copy(sourceNode, destParent, destIndex);
    else
      bookmarkModel_->Move(sourceNode, destParent, destIndex);
  }

  [self closeFolderAndStopTrackingMenus];

  // Movement of a node triggers observers (like us) to rebuild the
  // bar so we don't have to do so explicitly.

  return YES;
}

- (void)draggingEnded:(id<NSDraggingInfo>)info {
  [self closeFolderAndStopTrackingMenus];
  [[BookmarkButton draggedButton] setHidden:NO];
  [self resetAllButtonPositionsWithAnimation:YES];
}

// Set insertionPos_ and hasInsertionPos_, and make insertion space for a
// hypothetical drop with the new button having a left edge of |where|.
// Gets called only by our view.
- (void)setDropInsertionPos:(CGFloat)where {
  if (!hasInsertionPos_ || where != insertionPos_) {
    insertionPos_ = where;
    hasInsertionPos_ = YES;
    CGFloat left = bookmarks::kBookmarkHorizontalPadding;
    CGFloat paddingWidth = bookmarks::kDefaultBookmarkWidth;
    BookmarkButton* draggedButton = [BookmarkButton draggedButton];
    if (draggedButton) {
      paddingWidth = std::min(bookmarks::kDefaultBookmarkWidth,
                              NSWidth([draggedButton frame]));
    }
    // Put all the buttons where they belong, with all buttons to the right
    // of the insertion point shuffling right to make space for it.
    for (NSButton* button in buttons_.get()) {
      // Hidden buttons get no space.
      if ([button isHidden])
        continue;
      NSRect buttonFrame = [button frame];
      buttonFrame.origin.x = left;
      // Update "left" for next time around.
      left += buttonFrame.size.width;
      if (left > insertionPos_)
        buttonFrame.origin.x += paddingWidth;
      left += bookmarks::kBookmarkHorizontalPadding;
      if (innerContentAnimationsEnabled_)
        [[button animator] setFrame:buttonFrame];
      else
        [button setFrame:buttonFrame];
    }
  }
}

// Put all visible bookmark bar buttons in their normal locations, either with
// or without animation according to the |animate| flag.
// This is generally useful, so is called from various places internally.
- (void)resetAllButtonPositionsWithAnimation:(BOOL)animate {
  CGFloat left = bookmarks::kBookmarkHorizontalPadding;
  animate &= innerContentAnimationsEnabled_;

  for (NSButton* button in buttons_.get()) {
    // Hidden buttons get no space.
    if ([button isHidden])
      continue;
    NSRect buttonFrame = [button frame];
    buttonFrame.origin.x = left;
    left += buttonFrame.size.width + bookmarks::kBookmarkHorizontalPadding;
    if (animate)
      [[button animator] setFrame:buttonFrame];
    else
      [button setFrame:buttonFrame];
  }
}

// Clear insertion flag, remove insertion space and put all visible bookmark
// bar buttons in their normal locations.
// Gets called only by our view.
- (void)clearDropInsertionPos {
  if (hasInsertionPos_) {
    hasInsertionPos_ = NO;
    [self resetAllButtonPositionsWithAnimation:YES];
  }
}

#pragma mark Bridge Notification Handlers

// TODO(jrg): for now this is brute force.
- (void)loaded:(BookmarkModel*)model {
  DCHECK(model == bookmarkModel_);
  if (!model->IsLoaded())
    return;

  // If this is a rebuild request while we have a folder open, close it.
  // TODO(mrossetti): Eliminate the need for this because it causes the folder
  // menu to disappear after a cut/copy/paste/delete change.
  // See: http://crbug.com/36614
  if (folderController_)
    [self closeAllBookmarkFolders];

  // Brute force nuke and build.
  savedFrameWidth_ = NSWidth([[self view] frame]);
  const BookmarkNode* node = model->bookmark_bar_node();
  [self clearBookmarkBar];
  [self addNodesToButtonList:node];
  [self createOtherBookmarksButton];
  [self updateTheme:[[[self view] window] themeProvider]];
  [self positionOffTheSideButton];
  [self addNonBookmarkButtonsToView];
  [self addButtonsToView];
  [self configureOffTheSideButtonContentsAndVisibility];
  [self setNodeForBarMenu];
  [self reconfigureBookmarkBar];
}

- (void)beingDeleted:(BookmarkModel*)model {
  // The browser may be being torn down; little is safe to do.  As an
  // example, it may not be safe to clear the pasteboard.
  // http://crbug.com/38665
}

- (void)nodeAdded:(BookmarkModel*)model
           parent:(const BookmarkNode*)newParent index:(int)newIndex {
  // If a context menu is open, close it.
  [self cancelMenuTracking];

  const BookmarkNode* newNode = newParent->GetChild(newIndex);
  id<BookmarkButtonControllerProtocol> newController =
      [self controllerForNode:newParent];
  [newController addButtonForNode:newNode atIndex:newIndex];
  // If we go from 0 --> 1 bookmarks we may need to hide the
  // "bookmarks go here" text container.
  [self showOrHideNoItemContainerForNode:model->bookmark_bar_node()];
  // Cope with chevron or "Other Bookmarks" buttons possibly changing state.
  [self reconfigureBookmarkBar];
}

// TODO(jrg): for now this is brute force.
- (void)nodeChanged:(BookmarkModel*)model
               node:(const BookmarkNode*)node {
  [self loaded:model];
}

- (void)nodeMoved:(BookmarkModel*)model
        oldParent:(const BookmarkNode*)oldParent oldIndex:(int)oldIndex
        newParent:(const BookmarkNode*)newParent newIndex:(int)newIndex {
  const BookmarkNode* movedNode = newParent->GetChild(newIndex);
  id<BookmarkButtonControllerProtocol> oldController =
      [self controllerForNode:oldParent];
  id<BookmarkButtonControllerProtocol> newController =
      [self controllerForNode:newParent];
  if (newController == oldController) {
    [oldController moveButtonFromIndex:oldIndex toIndex:newIndex];
  } else {
    [oldController removeButton:oldIndex animate:NO];
    [newController addButtonForNode:movedNode atIndex:newIndex];
  }
  // If the bar is one of the parents we may need to update the visibility
  // of the "bookmarks go here" presentation.
  [self showOrHideNoItemContainerForNode:model->bookmark_bar_node()];
  // Cope with chevron or "Other Bookmarks" buttons possibly changing state.
  [self reconfigureBookmarkBar];
}

- (void)nodeRemoved:(BookmarkModel*)model
             parent:(const BookmarkNode*)oldParent index:(int)index {
  // If a context menu is open, close it.
  [self cancelMenuTracking];

  // Locate the parent node. The parent may not be showing, in which case
  // we do nothing.
  id<BookmarkButtonControllerProtocol> parentController =
      [self controllerForNode:oldParent];
  [parentController removeButton:index animate:YES];
  // If we go from 1 --> 0 bookmarks we may need to show the
  // "bookmarks go here" text container.
  [self showOrHideNoItemContainerForNode:model->bookmark_bar_node()];
  // If we deleted the only item on the "off the side" menu we no
  // longer need to show it.
  [self reconfigureBookmarkBar];
}

// TODO(jrg): linear searching is bad.
// Need a BookmarkNode-->NSCell mapping.
//
// TODO(jrg): if the bookmark bar is open on launch, we see the
// buttons all placed, then "scooted over" as the favicons load.  If
// this looks bad I may need to change widthForBookmarkButtonCell to
// add space for an image even if not there on the assumption that
// favicons will eventually load.
- (void)nodeFaviconLoaded:(BookmarkModel*)model
                     node:(const BookmarkNode*)node {
  for (BookmarkButton* button in buttons_.get()) {
    const BookmarkNode* cellnode = [button bookmarkNode];
    if (cellnode == node) {
      [[button cell] setBookmarkCellText:[button title]
                                   image:[self faviconForNode:node]];
      // Adding an image means we might need more room for the
      // bookmark.  Test for it by growing the button (if needed)
      // and shifting everything else over.
      [self checkForBookmarkButtonGrowth:button];
      return;
    }
  }

  if (folderController_)
    [folderController_ faviconLoadedForNode:node];
}

// TODO(jrg): for now this is brute force.
- (void)nodeChildrenReordered:(BookmarkModel*)model
                         node:(const BookmarkNode*)node {
  [self loaded:model];
}

#pragma mark BookmarkBarState Protocol

// (BookmarkBarState protocol)
- (BOOL)isVisible {
  if ([self shouldShowAtBottomWhenDetached] &&
      state_ == BookmarkBar::DETACHED &&
      [self currentTabContentsHeight] <
          chrome::search::kMinContentHeightForBottomBookmarkBar) {
    return NO;
  }

  return barIsEnabled_ && (state_ == BookmarkBar::SHOW ||
                           state_ == BookmarkBar::DETACHED ||
                           lastState_ == BookmarkBar::SHOW ||
                           lastState_ == BookmarkBar::DETACHED);
}

// (BookmarkBarState protocol)
- (BOOL)isInState:(BookmarkBar::State)state {
  return state_ == state && ![self isAnimationRunning];
}

// (BookmarkBarState protocol)
- (BOOL)isAnimatingToState:(BookmarkBar::State)state {
  return state_ == state && [self isAnimationRunning];
}

// (BookmarkBarState protocol)
- (BOOL)isAnimatingFromState:(BookmarkBar::State)state {
  return lastState_ == state && [self isAnimationRunning];
}

// (BookmarkBarState protocol)
- (BOOL)isAnimatingFromState:(BookmarkBar::State)fromState
                     toState:(BookmarkBar::State)toState {
  return lastState_ == fromState &&
         state_ == toState &&
         [self isAnimationRunning];
}

// (BookmarkBarState protocol)
- (BOOL)isAnimatingBetweenState:(BookmarkBar::State)fromState
                       andState:(BookmarkBar::State)toState {
  return [self isAnimatingFromState:fromState toState:toState] ||
         [self isAnimatingFromState:toState toState:fromState];
}

// (BookmarkBarState protocol)
- (CGFloat)detachedMorphProgress {
  if ([self isInState:BookmarkBar::DETACHED]) {
    return 1;
  }
  if ([self isAnimatingToState:BookmarkBar::DETACHED]) {
    return static_cast<CGFloat>(
        [[self animatableView] currentAnimationProgress]);
  }
  if ([self isAnimatingFromState:BookmarkBar::DETACHED]) {
    return static_cast<CGFloat>(
        1 - [[self animatableView] currentAnimationProgress]);
  }
  return 0;
}

#pragma mark BookmarkBarToolbarViewController Protocol

- (int)currentTabContentsHeight {
  BrowserWindowController* browserController =
      [BrowserWindowController browserWindowControllerForView:[self view]];
  return NSHeight([[browserController tabContentArea] frame]);
}

- (ui::ThemeProvider*)themeProvider {
  return ThemeServiceFactory::GetForProfile(browser_->profile());
}

- (BOOL)shouldShowAtBottomWhenDetached {
  return NO;
}

#pragma mark BookmarkButtonDelegate Protocol

- (void)fillPasteboard:(NSPasteboard*)pboard
       forDragOfButton:(BookmarkButton*)button {
  [[self folderTarget] fillPasteboard:pboard forDragOfButton:button];
}

// BookmarkButtonDelegate protocol implementation.  When menus are
// "active" (e.g. you clicked to open one), moving the mouse over
// another folder button should close the 1st and open the 2nd (like
// real menus).  We detect and act here.
- (void)mouseEnteredButton:(id)sender event:(NSEvent*)event {
  DCHECK([sender isKindOfClass:[BookmarkButton class]]);

  // If folder menus are not being shown, do nothing.  This is different from
  // BookmarkBarFolderController's implementation because the bar should NOT
  // automatically open folder menus when the mouse passes over a folder
  // button while the BookmarkBarFolderController DOES automically open
  // a subfolder menu.
  if (!showFolderMenus_)
    return;

  // From here down: same logic as BookmarkBarFolderController.
  // TODO(jrg): find a way to share these 4 non-comment lines?
  // http://crbug.com/35966
  // If already opened, then we exited but re-entered the button, so do nothing.
  if ([folderController_ parentButton] == sender)
    return;
  // Else open a new one if it makes sense to do so.
  if ([sender bookmarkNode]->is_folder()) {
    // Update |hoverButton_| so that it corresponds to the open folder.
    hoverButton_.reset([sender retain]);
    [folderTarget_ openBookmarkFolderFromButton:sender];
  } else {
    // We're over a non-folder bookmark so close any old folders.
    [folderController_ close];
    folderController_ = nil;
  }
}

// BookmarkButtonDelegate protocol implementation.
- (void)mouseExitedButton:(id)sender event:(NSEvent*)event {
  // Don't care; do nothing.
  // This is different behavior that the folder menus.
}

- (NSWindow*)browserWindow {
  return [[self view] window];
}

- (BOOL)canDragBookmarkButtonToTrash:(BookmarkButton*)button {
  return [self canEditBookmarks] &&
         [self canEditBookmark:[button bookmarkNode]];
}

- (void)didDragBookmarkToTrash:(BookmarkButton*)button {
  if ([self canDragBookmarkButtonToTrash:button]) {
    const BookmarkNode* node = [button bookmarkNode];
    if (node) {
      const BookmarkNode* parent = node->parent();
      bookmarkModel_->Remove(parent,
                             parent->GetIndexOf(node));
    }
  }
}

- (void)bookmarkDragDidEnd:(BookmarkButton*)button
                 operation:(NSDragOperation)operation {
  [button setHidden:NO];
  [self resetAllButtonPositionsWithAnimation:YES];
}


#pragma mark BookmarkButtonControllerProtocol

// Close all bookmark folders.  "Folder" here is the fake menu for
// bookmark folders, not a button context menu.
- (void)closeAllBookmarkFolders {
  [self watchForExitEvent:NO];
  [folderController_ close];
  folderController_ = nil;
}

- (void)closeBookmarkFolder:(id)sender {
  // We're the top level, so close one means close them all.
  [self closeAllBookmarkFolders];
}

- (BookmarkModel*)bookmarkModel {
  return bookmarkModel_;
}

- (BOOL)draggingAllowed:(id<NSDraggingInfo>)info {
  return [self canEditBookmarks];
}

// TODO(jrg): much of this logic is duped with
// [BookmarkBarFolderController draggingEntered:] except when noted.
// http://crbug.com/35966
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info {
  NSPoint point = [info draggingLocation];
  BookmarkButton* button = [self buttonForDroppingOnAtPoint:point];

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
    DCHECK([[hoverButton_ target]
            respondsToSelector:@selector(openBookmarkFolderFromButton:)]);
    [[hoverButton_ target]
     performSelector:@selector(openBookmarkFolderFromButton:)
     withObject:hoverButton_
     afterDelay:bookmarks::kDragHoverOpenDelay
     inModes:[NSArray arrayWithObject:NSRunLoopCommonModes]];
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
  // Only close the folder menu if the user dragged up past the BMB. If the user
  // dragged to below the BMB, they might be trying to drop a link into the open
  // folder menu.
  // TODO(asvitkine): Need a way to close the menu if the user dragged below but
  //                  not into the menu.
  NSRect bounds = [[self view] bounds];
  NSPoint origin = [[self view] convertPoint:bounds.origin toView:nil];
  if ([info draggingLocation].y > origin.y + bounds.size.height)
    [self closeFolderAndStopTrackingMenus];

  // NOT the same as a cancel --> we may have moved the mouse into the submenu.
  if (hoverButton_) {
    [NSObject cancelPreviousPerformRequestsWithTarget:[hoverButton_ target]];
    hoverButton_.reset();
  }
}

- (BOOL)dragShouldLockBarVisibility {
  return ![self isInState:BookmarkBar::DETACHED] &&
  ![self isAnimatingToState:BookmarkBar::DETACHED];
}

// TODO(mrossetti,jrg): Yet more code dup with BookmarkBarFolderController.
// http://crbug.com/35966
- (BOOL)dragButton:(BookmarkButton*)sourceButton
                to:(NSPoint)point
              copy:(BOOL)copy {
  DCHECK([sourceButton isKindOfClass:[BookmarkButton class]]);
  const BookmarkNode* sourceNode = [sourceButton bookmarkNode];
  return [self dragBookmark:sourceNode to:point copy:copy];
}

- (BOOL)dragBookmarkData:(id<NSDraggingInfo>)info {
  BOOL dragged = NO;
  std::vector<const BookmarkNode*> nodes([self retrieveBookmarkNodeData]);
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

- (std::vector<const BookmarkNode*>)retrieveBookmarkNodeData {
  std::vector<const BookmarkNode*> dragDataNodes;
  BookmarkNodeData dragData;
  if(dragData.ReadFromDragClipboard()) {
    BookmarkModel* bookmarkModel = [self bookmarkModel];
    Profile* profile = bookmarkModel->profile();
    std::vector<const BookmarkNode*> nodes(dragData.GetNodes(profile));
    dragDataNodes.assign(nodes.begin(), nodes.end());
  }
  return dragDataNodes;
}

// Return YES if we should show the drop indicator, else NO.
- (BOOL)shouldShowIndicatorShownForPoint:(NSPoint)point {
  return ![self buttonForDroppingOnAtPoint:point];
}

// Return the x position for a drop indicator.
- (CGFloat)indicatorPosForDragToPoint:(NSPoint)point {
  CGFloat x = 0;
  int destIndex = [self indexForDragToPoint:point];
  int numButtons = displayedButtonCount_;

  // If it's a drop strictly between existing buttons ...

  if (destIndex == 0) {
    x = 0.5 * bookmarks::kBookmarkHorizontalPadding;
  } else if (destIndex > 0 && destIndex < numButtons) {
    // ... put the indicator right between the buttons.
    BookmarkButton* button =
        [buttons_ objectAtIndex:static_cast<NSUInteger>(destIndex-1)];
    DCHECK(button);
    NSRect buttonFrame = [button frame];
    x = NSMaxX(buttonFrame) + 0.5 * bookmarks::kBookmarkHorizontalPadding;

    // If it's a drop at the end (past the last button, if there are any) ...
  } else if (destIndex == numButtons) {
    // and if it's past the last button ...
    if (numButtons > 0) {
      // ... find the last button, and put the indicator to its right.
      BookmarkButton* button =
          [buttons_ objectAtIndex:static_cast<NSUInteger>(destIndex - 1)];
      DCHECK(button);
      NSRect buttonFrame = [button frame];
      x = NSMaxX(buttonFrame) + 0.5 * bookmarks::kBookmarkHorizontalPadding;

      // Otherwise, put it right at the beginning.
    } else {
      x = 0.5 * bookmarks::kBookmarkHorizontalPadding;
    }
  } else {
    NOTREACHED();
  }

  return x;
}

- (void)childFolderWillShow:(id<BookmarkButtonControllerProtocol>)child {
  // If the bookmarkbar is not in detached mode, lock bar visibility, forcing
  // the overlay to stay open when in fullscreen mode.
  if (![self isInState:BookmarkBar::DETACHED] &&
      ![self isAnimatingToState:BookmarkBar::DETACHED]) {
    BrowserWindowController* browserController =
        [BrowserWindowController browserWindowControllerForView:[self view]];
    [browserController lockBarVisibilityForOwner:child
                                   withAnimation:NO
                                           delay:NO];
  }
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

// Add a new folder controller as triggered by the given folder button.
- (void)addNewFolderControllerWithParentButton:(BookmarkButton*)parentButton {

  // If doing a close/open, make sure the fullscreen chrome doesn't
  // have a chance to begin animating away in the middle of things.
  BrowserWindowController* browserController =
      [BrowserWindowController browserWindowControllerForView:[self view]];
  // Confirm we're not re-locking with ourself as an owner before locking.
  DCHECK([browserController isBarVisibilityLockedForOwner:self] == NO);
  [browserController lockBarVisibilityForOwner:self
                                 withAnimation:NO
                                         delay:NO];

  if (folderController_)
    [self closeAllBookmarkFolders];

  // Folder controller, like many window controllers, owns itself.
  folderController_ =
      [[BookmarkBarFolderController alloc] initWithParentButton:parentButton
                                               parentController:nil
                                                  barController:self];
  [folderController_ showWindow:self];

  // Only BookmarkBarController has this; the
  // BookmarkBarFolderController does not.
  [self watchForExitEvent:YES];

  // No longer need to hold the lock; the folderController_ now owns it.
  [browserController releaseBarVisibilityForOwner:self
                                    withAnimation:NO
                                            delay:NO];
}

- (void)openAll:(const BookmarkNode*)node
    disposition:(WindowOpenDisposition)disposition {
  [self closeFolderAndStopTrackingMenus];
  chrome::OpenAll([[self view] window], browser_, node, disposition,
                  browser_->profile());
}

- (void)addButtonForNode:(const BookmarkNode*)node
                 atIndex:(NSInteger)buttonIndex {
  int newOffset = 0;
  if (buttonIndex == -1)
    buttonIndex = [buttons_ count];  // New button goes at the end.
  if (buttonIndex <= (NSInteger)[buttons_ count]) {
    if (buttonIndex) {
      BookmarkButton* targetButton = [buttons_ objectAtIndex:buttonIndex - 1];
      NSRect targetFrame = [targetButton frame];
      newOffset = targetFrame.origin.x + NSWidth(targetFrame) +
          bookmarks::kBookmarkHorizontalPadding;
    }
    BookmarkButton* newButton = [self buttonForNode:node xOffset:&newOffset];
    ++displayedButtonCount_;
    [buttons_ insertObject:newButton atIndex:buttonIndex];
    [buttonView_ addSubview:newButton];
    [self resetAllButtonPositionsWithAnimation:NO];
    // See if any buttons need to be pushed off to or brought in from the side.
    [self reconfigureBookmarkBar];
  } else  {
    // A button from somewhere else (not the bar) is being moved to the
    // off-the-side so insure it gets redrawn if its showing.
    [self reconfigureBookmarkBar];
    [folderController_ reconfigureMenu];
  }
}

// TODO(mrossetti): Duplicate code with BookmarkBarFolderController.
// http://crbug.com/35966
- (BOOL)addURLs:(NSArray*)urls withTitles:(NSArray*)titles at:(NSPoint)point {
  DCHECK([urls count] == [titles count]);
  BOOL nodesWereAdded = NO;
  // Figure out where these new bookmarks nodes are to be added.
  BookmarkButton* button = [self buttonForDroppingOnAtPoint:point];
  const BookmarkNode* destParent = NULL;
  int destIndex = 0;
  if ([button isFolder]) {
    destParent = [button bookmarkNode];
    // Drop it at the end.
    destIndex = [button bookmarkNode]->child_count();
  } else {
    // Else we're dropping somewhere on the bar, so find the right spot.
    destParent = bookmarkModel_->bookmark_bar_node();
    destIndex = [self indexForDragToPoint:point];
  }

  // Don't add the bookmarks if the destination index shows an error.
  if (destIndex >= 0) {
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
        bookmarkModel_->AddURL(destParent,
                               destIndex++,
                               base::SysNSStringToUTF16(
                                  [titles objectAtIndex:i]),
                               gurl);
        nodesWereAdded = YES;
      }
    }
  }
  return nodesWereAdded;
}

- (void)moveButtonFromIndex:(NSInteger)fromIndex toIndex:(NSInteger)toIndex {
  if (fromIndex != toIndex) {
    NSInteger buttonCount = (NSInteger)[buttons_ count];
    if (toIndex == -1)
      toIndex = buttonCount;
    // See if we have a simple move within the bar, which will be the case if
    // both button indexes are in the visible space.
    if (fromIndex < buttonCount && toIndex < buttonCount) {
      BookmarkButton* movedButton = [buttons_ objectAtIndex:fromIndex];
      [buttons_ removeObjectAtIndex:fromIndex];
      [buttons_ insertObject:movedButton atIndex:toIndex];
      [movedButton setHidden:NO];
      [self resetAllButtonPositionsWithAnimation:NO];
    } else if (fromIndex < buttonCount) {
      // A button is being removed from the bar and added to off-the-side.
      // By now the node has already been inserted into the model so the
      // button to be added is represented by |toIndex|. Things get
      // complicated because the off-the-side is showing and must be redrawn
      // while possibly re-laying out the bookmark bar.
      [self removeButton:fromIndex animate:NO];
      [self reconfigureBookmarkBar];
      [folderController_ reconfigureMenu];
    } else if (toIndex < buttonCount) {
      // A button is being added to the bar and removed from off-the-side.
      // By now the node has already been inserted into the model so the
      // button to be added is represented by |toIndex|.
      const BookmarkNode* node = bookmarkModel_->bookmark_bar_node();
      const BookmarkNode* movedNode = node->GetChild(toIndex);
      DCHECK(movedNode);
      [self addButtonForNode:movedNode atIndex:toIndex];
      [self reconfigureBookmarkBar];
    } else {
      // A button is being moved within the off-the-side.
      fromIndex -= buttonCount;
      toIndex -= buttonCount;
      [folderController_ moveButtonFromIndex:fromIndex toIndex:toIndex];
    }
  }
}

- (void)removeButton:(NSInteger)buttonIndex animate:(BOOL)animate {
  if (buttonIndex < (NSInteger)[buttons_ count]) {
    // The button being removed is showing in the bar.
    BookmarkButton* oldButton = [buttons_ objectAtIndex:buttonIndex];
    if (oldButton == [folderController_ parentButton]) {
      // If we are deleting a button whose folder is currently open, close it!
      [self closeAllBookmarkFolders];
    }
    if (animate && innerContentAnimationsEnabled_ && [self isVisible] &&
        [[self browserWindow] isMainWindow]) {
      NSPoint poofPoint = [oldButton screenLocationForRemoveAnimation];
      NSShowAnimationEffect(NSAnimationEffectDisappearingItemDefault, poofPoint,
                            NSZeroSize, nil, nil, nil);
    }
    [oldButton setDelegate:nil];
    [oldButton removeFromSuperview];
    [buttons_ removeObjectAtIndex:buttonIndex];
    --displayedButtonCount_;
    [self resetAllButtonPositionsWithAnimation:YES];
    [self reconfigureBookmarkBar];
  } else if (folderController_ &&
             [folderController_ parentButton] == offTheSideButton_) {
    // The button being removed is in the OTS (off-the-side) and the OTS
    // menu is showing so we need to remove the button.
    NSInteger index = buttonIndex - displayedButtonCount_;
    [folderController_ removeButton:index animate:YES];
  }
}

- (id<BookmarkButtonControllerProtocol>)controllerForNode:
    (const BookmarkNode*)node {
  // See if it's in the bar, then if it is in the hierarchy of visible
  // folder menus.
  if (bookmarkModel_->bookmark_bar_node() == node)
    return self;
  return [folderController_ controllerForNode:node];
}

#pragma mark TestingAPI Only

- (NSMenu*)buttonContextMenu {
  return buttonContextMenu_;
}

// Intentionally ignores ownership issues; used for testing and we try
// to minimize touching the object passed in (likely a mock).
- (void)setButtonContextMenu:(id)menu {
  buttonContextMenu_ = menu;
}

@end
