// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_controller.h"

#include <stddef.h>

#import "base/mac/bundle_locations.h"
#import "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_stats.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#import "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#import "chrome/browser/ui/cocoa/background_gradient_view.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_bridge.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_folder_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_folder_window.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_toolbar_view.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_view_cocoa.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_button.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_button_cell.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_context_menu_cocoa_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_editor_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_folder_target.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_cocoa_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_model_observer_for_cocoa.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_name_folder_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/menu_button.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#import "chrome/browser/ui/cocoa/view_id_util.h"
#import "chrome/browser/ui/cocoa/view_resizer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "ui/base/clipboard/clipboard_util_mac.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#import "ui/base/cocoa/nsview_additions.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/resources/grit/ui_resources.h"

using base::UserMetricsAction;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::BookmarkNodeData;
using content::OpenURLParams;
using content::Referrer;
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

// Duration of the bookmark bar animations.
const NSTimeInterval kBookmarkBarAnimationDuration = 0.12;
const NSTimeInterval kDragAndDropAnimationDuration = 0.25;

void RecordAppLaunch(Profile* profile, GURL url) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->
          enabled_extensions().GetAppByURL(url);
  if (!extension)
    return;

  extensions::RecordAppLaunchType(extension_misc::APP_LAUNCH_BOOKMARK_BAR,
                                  extension->GetType());
}

}  // namespace

@interface BookmarkBarController ()

// Updates the sizes and positions of the subviews.
- (void)layoutSubviews;

// Moves to the given next state (from the current state), possibly animating.
// If |animate| is NO, it will stop any running animation and jump to the given
// state. If YES, it may either (depending on implementation) jump to the end of
// the current animation and begin the next one, or stop the current animation
// mid-flight and animate to the next state.
- (void)moveToState:(BookmarkBar::State)nextState
      withAnimation:(BOOL)animate;

// Create buttons for all items in the given bookmark node tree.
// Modifies self->buttons_.  Do not add more buttons than will fit on the view.
- (void)addNodesToButtonList:(const BookmarkNode*)node;

// Create an autoreleased button appropriate for insertion into the bookmark
// bar. Update |xOffset| with the offset appropriate for the subsequent button.
- (BookmarkButton*)buttonForNode:(const BookmarkNode*)node
                         xOffset:(int*)xOffset;

// Find a parent whose button is visible on the bookmark bar.
- (BookmarkButton*)bookmarkButtonToPulseForNode:(const BookmarkNode*)node;

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

- (int)preferredHeight;
- (void)addButtonsToView;
- (BOOL)setManagedBookmarksButtonVisibility;
- (BOOL)setSupervisedBookmarksButtonVisibility;
- (BOOL)setOtherBookmarksButtonVisibility;
- (BOOL)setAppsPageShortcutButtonVisibility;
- (BookmarkButton*)createCustomBookmarkButtonForCell:(NSCell*)cell;
- (void)createManagedBookmarksButton;
- (void)createSupervisedBookmarksButton;
- (void)createOtherBookmarksButton;
- (void)createAppsPageShortcutButton;
- (void)openAppsPage:(id)sender;
- (void)centerNoItemsLabel;
- (void)positionRightSideButtons;
- (void)watchForExitEvent:(BOOL)watch;
- (void)resetAllButtonPositionsWithAnimation:(BOOL)animate;

@end

@implementation BookmarkBarController

@synthesize currentState = currentState_;
@synthesize lastState = lastState_;
@synthesize isAnimationRunning = isAnimationRunning_;
@synthesize delegate = delegate_;
@synthesize stateAnimationsEnabled = stateAnimationsEnabled_;
@synthesize innerContentAnimationsEnabled = innerContentAnimationsEnabled_;

- (id)initWithBrowser:(Browser*)browser
         initialWidth:(CGFloat)initialWidth
             delegate:(id<BookmarkBarControllerDelegate>)delegate {
  if ((self = [super initWithNibName:@"BookmarkBar"
                              bundle:base::mac::FrameworkBundle()])) {
    currentState_ = BookmarkBar::HIDDEN;
    lastState_ = BookmarkBar::HIDDEN;

    browser_ = browser;
    initialWidth_ = initialWidth;
    bookmarkModel_ =
        BookmarkModelFactory::GetForBrowserContext(browser_->profile());
    managedBookmarkService_ =
        ManagedBookmarkServiceFactory::GetForProfile(browser_->profile());
    buttons_.reset([[NSMutableArray alloc] init]);
    delegate_ = delegate;
    folderTarget_.reset(
        [[BookmarkFolderTarget alloc] initWithController:self
                                                 profile:browser_->profile()]);

    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    folderImage_.reset(
        rb.GetNativeImageNamed(IDR_BOOKMARK_BAR_FOLDER).CopyNSImage());
    folderImageWhite_.reset(
        rb.GetNativeImageNamed(IDR_BOOKMARK_BAR_FOLDER_WHITE).CopyNSImage());

    const int kIconSize = 16;
    defaultImage_.reset([NSImageFromImageSkia(gfx::CreateVectorIcon(
        omnibox::kHttpIcon, kIconSize, gfx::kChromeIconGrey)) retain]);
    defaultImageIncognito_.reset([NSImageFromImageSkia(gfx::CreateVectorIcon(
        omnibox::kHttpIcon, kIconSize, SkColorSetA(SK_ColorWHITE, 0xCC)))
        retain]);

    innerContentAnimationsEnabled_ = YES;
    stateAnimationsEnabled_ = YES;

    // Register for theme changes, bookmark button pulsing, ...
    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserver:self
                      selector:@selector(themeDidChangeNotification:)
                          name:kBrowserThemeDidChangeNotification
                        object:nil];

    contextMenuController_.reset(
        [[BookmarkContextMenuCocoaController alloc]
            initWithBookmarkBarController:self]);
  }
  return self;
}

- (Browser*)browser {
  return browser_;
}

- (BookmarkBarToolbarView*)controlledView {
  return base::mac::ObjCCastStrict<BookmarkBarToolbarView>([self view]);
}

- (BookmarkContextMenuCocoaController*)menuController {
  return contextMenuController_.get();
}

- (void)loadView {
  // Height is 0 because this is what the superview expects
  [self setView:[[[BookmarkBarToolbarView alloc]
                    initWithFrame:NSMakeRect(0, 0, initialWidth_, 0)]
                    autorelease]];
  [[self view] setHidden:YES];
  [[self view] setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin];
  [[self controlledView] setController:self];
  [[self controlledView] setDelegate:self];

  buttonView_.reset([[BookmarkBarView alloc]
      initWithController:self
                   frame:NSMakeRect(0, -2, 584, 144)]);
  [buttonView_ setAutoresizingMask:NSViewWidthSizable | NSViewMinYMargin |
                                   NSViewMaxXMargin];
  [[buttonView_ importBookmarksButton] setTarget:self];
  [[buttonView_ importBookmarksButton] setAction:@selector(importBookmarks:)];

  [self createOffTheSideButton];
  [buttonView_ addSubview:offTheSideButton_];

  [self.view addSubview:buttonView_];
  // viewDidLoad became part of the API in 10.10
  if (!base::mac::IsAtLeastOS10_10())
    [self viewDidLoad];
}

- (BookmarkButton*)bookmarkButtonToPulseForNode:(const BookmarkNode*)node {
  // Find the closest parent that is visible on the bar.
  while (node) {
    // Check if we've reached one of the special buttons. Otherwise, if the next
    // parent is the boomark bar, find the corresponding button.
    if ([managedBookmarksButton_ bookmarkNode] == node)
      return managedBookmarksButton_;

    if ([supervisedBookmarksButton_ bookmarkNode] == node)
      return supervisedBookmarksButton_;

    if ([otherBookmarksButton_ bookmarkNode] == node)
      return otherBookmarksButton_;

    if ([offTheSideButton_ bookmarkNode] == node)
      return offTheSideButton_;

    if (node->parent() == bookmarkModel_->bookmark_bar_node()) {
      for (BookmarkButton* button in [self buttons]) {
        if ([button bookmarkNode] == node) {
          [button setPulseIsStuckOn:YES];
          return button;
        }
      }
    }

    node = node->parent();
  }
  NOTREACHED();
  return nil;
}

- (void)startPulsingBookmarkNode:(const BookmarkNode*)node {
  [self stopPulsingBookmarkNode];

  pulsingButton_.reset([self bookmarkButtonToPulseForNode:node],
                       base::scoped_policy::RETAIN);
  if (!pulsingButton_)
    return;

  [pulsingButton_ setPulseIsStuckOn:YES];
  pulsingBookmarkObserver_.reset(
      new BookmarkModelObserverForCocoa(bookmarkModel_, ^() {
        // Stop pulsing if anything happened to the node.
        [self stopPulsingBookmarkNode];
      }));
  pulsingBookmarkObserver_->StartObservingNode(node);
}

- (void)stopPulsingBookmarkNode {
  if (!pulsingButton_)
    return;

  [pulsingButton_ setPulseIsStuckOn:NO];
  pulsingButton_.reset();
  pulsingBookmarkObserver_.reset();
}

- (void)dealloc {
  [buttonView_ setController:nil];
  [[self controlledView] setController:nil];
  [[self controlledView] setDelegate:nil];
  [self browserWillBeDestroyed];
  [super dealloc];
}

- (void)browserWillBeDestroyed {
  // If |bridge_| is null it means -viewDidLoad has not yet been called, which
  // can only happen if the nib wasn't loaded. Retrieving it via -[self view]
  // would load it now, but it's too late for that, so let it be nil. Note this
  // should only happen in tests.
  BookmarkBarToolbarView* view = nil;
  if (bridge_)
    view = [self controlledView];

  // Clear delegate so it doesn't get called during stopAnimation.
  [view setResizeDelegate:nil];

  // We better stop any in-flight animation if we're being killed.
  [view stopAnimation];

  // Remove our view from its superview so it doesn't attempt to reference
  // it when the controller is gone.
  //TODO(dmaclach): Remove -- http://crbug.com/25845
  [view removeFromSuperview];

  // Be sure there is no dangling pointer.
  if ([view respondsToSelector:@selector(setController:)])
    [view performSelector:@selector(setController:) withObject:nil];

  // For safety, make sure the buttons can no longer call us.
  for (BookmarkButton* button in buttons_.get()) {
    [button setDelegate:nil];
    [button setTarget:nil];
    [button setAction:nil];
  }

  bridge_.reset(NULL);
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [self watchForExitEvent:NO];
  browser_ = nullptr;
}

- (void)viewDidLoad {
  // We are enabled by default.
  barIsEnabled_ = YES;

  // Remember the original sizes of the 'no items' and 'import bookmarks'
  // fields to aid in resizing when the window frame changes.
  originalNoItemsRect_ = [[buttonView_ noItemTextfield] frame];
  originalImportBookmarksRect_ = [[buttonView_ importBookmarksButton] frame];

  // Bookmark buttons start farther from the bookmark bar's left edge so
  // adjust the positions of the noItems and importBookmarks textfields.
  const CGFloat kBookmarksTextfieldOffsetX = 14;
  originalNoItemsRect_.origin.x += kBookmarksTextfieldOffsetX;
  [[buttonView_ noItemTextfield] setFrame:originalNoItemsRect_];

  originalImportBookmarksRect_.origin.x += kBookmarksTextfieldOffsetX;
  [[buttonView_ importBookmarksButton] setFrame:originalImportBookmarksRect_];

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
             name:NSWindowWillEnterFullScreenNotification
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(willEnterOrLeaveFullscreen:)
             name:NSWindowWillExitFullScreenNotification
           object:nil];

  // Don't pass ourself along (as 'self') until our init is completely
  // done.  Thus, this call is (almost) last.
  bridge_.reset(new BookmarkBarBridge(browser_->profile(), self,
                                      bookmarkModel_));
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

- (void)layoutToFrame:(NSRect)frame {
  // The view should be pinned to the top of the window with a flexible width.
  DCHECK_EQ(NSViewWidthSizable | NSViewMinYMargin,
            [[self view] autoresizingMask]);
  [[self view] setFrame:frame];
  [self layoutSubviews];
}

// Change the layout of the bookmark bar's subviews in response to a visibility
// change (e.g., show or hide the bar) or style change (attached or floating).
- (void)layoutSubviews {
  NSRect frame = [[self view] frame];
  NSRect buttonViewFrame = NSMakeRect(0, 0, NSWidth(frame), NSHeight(frame));

  // Add padding to the detached bookmark bar.
  // The state of our morph (if any); 1 is total bubble, 0 is the regular bar.
  CGFloat morph = [self detachedMorphProgress];
  CGFloat padding = 0;
  padding = bookmarks::kNTPBookmarkBarPadding;
  buttonViewFrame =
      NSInsetRect(buttonViewFrame, morph * padding, morph * padding);

  [buttonView_ setFrame:buttonViewFrame];

  // Update bookmark button backgrounds.
  if ([self isAnimationRunning]) {
    for (NSButton* button in buttons_.get())
      [button setNeedsDisplay:YES];
    // Update the apps and other buttons explicitly, since they are not in the
    // buttons_ array.
    [appsPageShortcutButton_ setNeedsDisplay:YES];
    [managedBookmarksButton_ setNeedsDisplay:YES];
    [supervisedBookmarksButton_ setNeedsDisplay:YES];
    [otherBookmarksButton_ setNeedsDisplay:YES];
  }
}

// We don't change a preference; we only change visibility. Preference changing
// (global state) is handled in |chrome::ToggleBookmarkBarWhenVisible()|. We
// simply update based on what we're told.
- (void)updateVisibility {
  [self showBookmarkBarWithAnimation:NO];
}

- (void)updateExtraButtonsVisibility {
  if (!appsPageShortcutButton_.get() ||
      !managedBookmarksButton_.get() ||
      !supervisedBookmarksButton_.get()) {
    return;
  }
  [self setAppsPageShortcutButtonVisibility];
  [self setManagedBookmarksButtonVisibility];
  [self setSupervisedBookmarksButtonVisibility];
  [self resetAllButtonPositionsWithAnimation:NO];
  [self reconfigureBookmarkBar];
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

  return [self isInState:BookmarkBar::SHOW] ? bookmarks::kBookmarkBarOverlap
                                            : 0;
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

- (NSImage*)faviconForNode:(const BookmarkNode*)node
             forADarkTheme:(BOOL)forADarkTheme {
  if (!node)
    return forADarkTheme ? defaultImageIncognito_ : defaultImage_;

  if (forADarkTheme) {
    if (node == managedBookmarkService_->managed_node()) {
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      return rb.GetNativeImageNamed(
          IDR_BOOKMARK_BAR_FOLDER_MANAGED_WHITE).ToNSImage();
    }

    if (node == managedBookmarkService_->supervised_node()) {
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      return rb.GetNativeImageNamed(
          IDR_BOOKMARK_BAR_FOLDER_SUPERVISED_WHITE).ToNSImage();
    }

    if (node->is_folder())
      return folderImageWhite_;
  } else {
    if (node == managedBookmarkService_->managed_node()) {
      // Most users never see this node, so the image is only loaded if needed.
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      return rb.GetNativeImageNamed(
          IDR_BOOKMARK_BAR_FOLDER_MANAGED).ToNSImage();
    }

    if (node == managedBookmarkService_->supervised_node()) {
      // Most users never see this node, so the image is only loaded if needed.
      ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      return rb.GetNativeImageNamed(
          IDR_BOOKMARK_BAR_FOLDER_SUPERVISED).ToNSImage();
    }

    if (node->is_folder())
      return folderImage_;
  }

  const gfx::Image& favicon = bookmarkModel_->GetFavicon(node);
  if (!favicon.IsEmpty())
    return favicon.ToNSImage();

  return forADarkTheme ? defaultImageIncognito_ : defaultImage_;
}

- (void)closeFolderAndStopTrackingMenus {
  showFolderMenus_ = NO;
  [self closeAllBookmarkFolders];
}

- (BOOL)canEditBookmarks {
  PrefService* prefs = browser_->profile()->GetPrefs();
  return prefs->GetBoolean(bookmarks::prefs::kEditBookmarksEnabled);
}

- (BOOL)canEditBookmark:(const BookmarkNode*)node {
  // Don't allow edit/delete of the permanent nodes.
  if (node == nil || bookmarkModel_->is_permanent_node(node) ||
      !managedBookmarkService_->CanBeEditedByUser(node)) {
    return NO;
  }
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

- (void)cleanupAfterMenuFlashThread:(id)sender {
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
  DCHECK(node);
  WindowOpenDisposition disposition =
      ui::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  RecordAppLaunch(browser_->profile(), node->url());
  [self openURL:node->url() disposition:disposition];

  if (!animate)
    [self closeFolderAndStopTrackingMenus];
  RecordBookmarkLaunch(node, [self bookmarkLaunchLocation]);
}

// Common function to open a bookmark folder of any type.
- (void)openBookmarkFolder:(id)sender {
  DCHECK([sender isKindOfClass:[BookmarkButton class]]);
  DCHECK([[sender cell] isKindOfClass:[BookmarkButtonCell class]]);

  // Only record the action if it's the initial folder being opened.
  if (!showFolderMenus_)
    RecordBookmarkFolderOpen([self bookmarkLaunchLocation]);
  showFolderMenus_ = !showFolderMenus_;

  // Middle click on chevron should not open bookmarks under it, instead just
  // open its folder menu.
  if (sender == offTheSideButton_.get()) {
    [[sender cell] setStartingChildIndex:displayedButtonCount_];
    NSEvent* event = [NSApp currentEvent];
    if ([event type] == NSOtherMouseUp) {
      [self openOrCloseBookmarkFolderForOffTheSideButton];
      return;
    }
  }
  // Toggle presentation of bar folder menus.
  [folderTarget_ openBookmarkFolderFromButton:sender];
}

- (void)openOrCloseBookmarkFolderForOffTheSideButton {
  // If clicked on already opened folder, then close it and return.
  if ([folderController_ parentButton] == offTheSideButton_)
    [self closeBookmarkFolder:self];
  else
    [self addNewFolderControllerWithParentButton:offTheSideButton_];
}

// Click on a bookmark folder button.
- (void)openBookmarkFolderFromButton:(id)sender {
  [self openBookmarkFolder:sender];
}

// Click on the "off the side" button (chevron), which opens like a folder
// button but isn't exactly a parent folder.
- (void)openOffTheSideFolderFromButton:(id)sender {
  [self openBookmarkFolder:sender];
}

- (void)importBookmarks:(id)sender {
  chrome::ShowImportDialog(browser_);
}

- (NSButton*)appsPageShortcutButton {
  return appsPageShortcutButton_;
}

- (NSButton*)offTheSideButton {
  return offTheSideButton_;
}

- (NSImage*)offTheSideButtonImage:(BOOL)forDarkMode {
  const int kIconSize = 8;
  SkColor vectorIconColor = forDarkMode ? SkColorSetA(SK_ColorWHITE, 0xCC)
                                        : gfx::kChromeIconGrey;
  return NSImageFromImageSkia(
      gfx::CreateVectorIcon(kOverflowChevronIcon, kIconSize, vectorIconColor));
}

#pragma mark Private Methods

// Called after a theme change took place, possibly for a different profile.
- (void)themeDidChangeNotification:(NSNotification*)notification {
  [self updateTheme:[[[self view] window] themeProvider]];
}

- (BookmarkLaunchLocation)bookmarkLaunchLocation {
  return currentState_ == BookmarkBar::DETACHED ?
      BOOKMARK_LAUNCH_LOCATION_DETACHED_BAR :
      BOOKMARK_LAUNCH_LOCATION_ATTACHED_BAR;
}

// Position the right-side buttons including the off-the-side chevron.
- (void)positionRightSideButtons {
  int maxX = NSMaxX([[self buttonView] bounds]) -
      bookmarks::kBookmarkHorizontalPadding;
  int right = maxX;

  int ignored = 0;
  NSRect frame = [self frameForBookmarkButtonFromCell:
      [otherBookmarksButton_ cell] xOffset:&ignored];
  if (![otherBookmarksButton_ isHidden]) {
    right -= NSWidth(frame);
    frame.origin.x = right;
  } else {
    frame.origin.x = maxX - NSWidth(frame);
  }
  [otherBookmarksButton_ setFrame:frame];

  frame = [offTheSideButton_ frame];
  frame.size.height = bookmarks::kBookmarkFolderButtonHeight;
  right -= frame.size.width;
  frame.origin.x = right;
  [offTheSideButton_ setFrame:frame];
}

// Configure the off-the-side button (e.g. specify the node range,
// check if we should enable or disable it, etc).
- (void)configureOffTheSideButtonContentsAndVisibility {
  [[offTheSideButton_ cell] setStartingChildIndex:displayedButtonCount_];
  [[offTheSideButton_ cell]
   setBookmarkNode:bookmarkModel_->bookmark_bar_node()];
  int bookmarkChildren = bookmarkModel_->bookmark_bar_node()->child_count();
  if (bookmarkChildren > displayedButtonCount_) {
    [offTheSideButton_ setHidden:NO];
    // Set the off the side button as needing re-display. This is needed to
    // avoid the button being shown with a black background the first time
    // it's displayed. See https://codereview.chromium.org/1630453002/ for
    // more context.
    [offTheSideButton_ setNeedsDisplay:YES];
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

  BookmarkBarToolbarView* view = [self controlledView];

  // Set our height immediately via -[AnimatableView setHeight:].
  [view setHeight:[self preferredHeight]];

  // Only show the divider if showing the normal bookmark bar.
  BOOL showsDivider = [self isInState:BookmarkBar::SHOW];
  [view setShowsDivider:showsDivider];

  // Make sure we're shown.
  [view setHidden:![self isVisible]];

  // Update everything else.
  [self layoutSubviews];
  [self frameDidChange];
}

// (Private)
- (BOOL)doBookmarkBarAnimation {
  BookmarkBarToolbarView* view = [self controlledView];
  if ([self isAnimatingFromState:BookmarkBar::HIDDEN
                         toState:BookmarkBar::SHOW]) {
    [view setShowsDivider:YES];
    [view setHidden:NO];
    // Height takes into account the extra height we have since the toolbar
    // only compresses when we're done.
    [view animateToNewHeight:(chrome::kMinimumBookmarkBarHeight -
                              bookmarks::kBookmarkBarOverlap)
                    duration:kBookmarkBarAnimationDuration];
  } else if ([self isAnimatingFromState:BookmarkBar::SHOW
                                toState:BookmarkBar::HIDDEN]) {
    [view setShowsDivider:YES];
    [view setHidden:NO];
    [view animateToNewHeight:0
                    duration:kBookmarkBarAnimationDuration];
  } else if ([self isAnimatingFromState:BookmarkBar::SHOW
                                toState:BookmarkBar::DETACHED]) {
    [view setShowsDivider:YES];
    [view setHidden:NO];
    [view animateToNewHeight:chrome::kNTPBookmarkBarHeight
                    duration:kBookmarkBarAnimationDuration];
  } else if ([self isAnimatingFromState:BookmarkBar::DETACHED
                                toState:BookmarkBar::SHOW]) {
    [view setShowsDivider:YES];
    [view setHidden:NO];
    // Height takes into account the extra height we have since the toolbar
    // only compresses when we're done.
    [view animateToNewHeight:(chrome::kMinimumBookmarkBarHeight -
                              bookmarks::kBookmarkBarOverlap)
                    duration:kBookmarkBarAnimationDuration];
  } else {
    // Oops! An animation we don't know how to handle.
    return NO;
  }

  return YES;
}

// Actually open the URL.  This is the last chance for a unit test to
// override.
- (void)openURL:(GURL)url disposition:(WindowOpenDisposition)disposition {
  OpenURLParams params(
      url, Referrer(), disposition, ui::PAGE_TRANSITION_AUTO_BOOKMARK,
      false);
  browser_->OpenURL(params);
}

- (int)preferredHeight {
  DCHECK(![self isAnimationRunning]);

  if (!barIsEnabled_)
    return 0;

  switch (currentState_) {
    case BookmarkBar::SHOW:
      return chrome::kMinimumBookmarkBarHeight;
    case BookmarkBar::DETACHED:
      return chrome::kNTPBookmarkBarHeight;
    case BookmarkBar::HIDDEN:
      return 0;
  }
}

// Return an appropriate width for the given bookmark button cell.
- (CGFloat)widthForBookmarkButtonCell:(NSCell*)cell {
  return std::min([cell cellSize].width, bookmarks::kDefaultBookmarkWidth);
}

// For the given root node of the bookmark bar, show or hide (as
// appropriate) the "no items" container (text which says "bookmarks
// go here").
- (void)showOrHideNoItemContainerForNode:(const BookmarkNode*)node {
  BOOL hideNoItemWarning = !node->empty();
  [[buttonView_ noItemContainer] setHidden:hideNoItemWarning];
}

// TODO(jrg): write a "build bar" so there is a nice spot for things
// like the contextual menu which is invoked when not over a
// bookmark.  On Safari that menu has a "new folder" option.
- (void)addNodesToButtonList:(const BookmarkNode*)node {
  [self showOrHideNoItemContainerForNode:node];

  CGFloat maxViewX = NSMaxX([[self view] bounds]);
  int xOffset =
      bookmarks::kBookmarkLeftMargin - bookmarks::kBookmarkHorizontalPadding;

  // Draw the apps bookmark if needed.
  if (![appsPageShortcutButton_ isHidden]) {
    NSRect frame =
        [self frameForBookmarkButtonFromCell:[appsPageShortcutButton_ cell]
                                     xOffset:&xOffset];
    [appsPageShortcutButton_ setFrame:frame];
  }

  // Draw the managed bookmark folder if needed.
  if (![managedBookmarksButton_ isHidden]) {
    xOffset += bookmarks::kBookmarkHorizontalPadding;
    NSRect frame =
        [self frameForBookmarkButtonFromCell:[managedBookmarksButton_ cell]
                                     xOffset:&xOffset];
    [managedBookmarksButton_ setFrame:frame];
  }

  // Draw the supervised bookmark folder if needed.
  if (![supervisedBookmarksButton_ isHidden]) {
    xOffset += bookmarks::kBookmarkHorizontalPadding;
    NSRect frame =
        [self frameForBookmarkButtonFromCell:[supervisedBookmarksButton_ cell]
                                     xOffset:&xOffset];
    [supervisedBookmarksButton_ setFrame:frame];
  }

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

  base::scoped_nsobject<BookmarkButton> button(
      [[BookmarkButton alloc] initWithFrame:frame]);
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
  const ui::ThemeProvider* themeProvider = [[[self view] window] themeProvider];
  if (themeProvider) {
    NSColor* color =
        themeProvider->GetNSColor(ThemeProperties::COLOR_BOOKMARK_TEXT);
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

// Shows or hides the Managed, Supervised, or Other Bookmarks button as
// appropriate, and returns whether it ended up visible.
- (BOOL)setBookmarkButtonVisibility:(BookmarkButton*)button
                            canShow:(BOOL)show
            resetAllButtonPositions:(BOOL)resetButtons {
  if (!button)
    return NO;

  BOOL visible = ![button bookmarkNode]->empty() && show;
  BOOL currentVisibility = ![button isHidden];
  if (currentVisibility != visible) {
    [button setHidden:!visible];
    if (resetButtons)
      [self resetAllButtonPositionsWithAnimation:NO];
  }
  return visible;
}

// Shows or hides the Managed Bookmarks button as appropriate, and returns
// whether it ended up visible.
- (BOOL)setManagedBookmarksButtonVisibility {
  PrefService* prefs = browser_->profile()->GetPrefs();
  BOOL prefIsSet =
      prefs->GetBoolean(bookmarks::prefs::kShowManagedBookmarksInBookmarkBar);
  return [self setBookmarkButtonVisibility:managedBookmarksButton_.get()
                                   canShow:prefIsSet
                   resetAllButtonPositions:YES];
}

// Shows or hides the Supervised Bookmarks button as appropriate, and returns
// whether it ended up visible.
- (BOOL)setSupervisedBookmarksButtonVisibility {
  return [self setBookmarkButtonVisibility:supervisedBookmarksButton_.get()
                                   canShow:YES
                   resetAllButtonPositions:YES];
}

// Shows or hides the Other Bookmarks button as appropriate, and returns
// whether it ended up visible.
- (BOOL)setOtherBookmarksButtonVisibility {
  return [self setBookmarkButtonVisibility:otherBookmarksButton_.get()
                                   canShow:YES
                   resetAllButtonPositions:NO];
}

// Shows or hides the Apps button as appropriate, and returns whether it ended
// up visible.
- (BOOL)setAppsPageShortcutButtonVisibility {
  if (!appsPageShortcutButton_.get())
    return NO;

  BOOL visible =
      bookmarkModel_->loaded() &&
      chrome::ShouldShowAppsShortcutInBookmarkBar(browser_->profile());
  [appsPageShortcutButton_ setHidden:!visible];
  return visible;
}

// Creates a bookmark bar button that does not correspond to a regular bookmark
// or folder. It is used by the "Other Bookmarks" and the "Apps" buttons.
- (BookmarkButton*)createCustomBookmarkButtonForCell:(NSCell*)cell {
  BookmarkButton* button = [[BookmarkButton alloc] init];
  [[button draggableButton] setDraggable:NO];
  [[button draggableButton] setActsOnMouseDown:YES];
  [button setCell:cell];
  [button setDelegate:self];
  [button setTarget:self];
  // Make sure this button, like all other BookmarkButtons, lives
  // until the end of the current event loop.
  [[button retain] autorelease];
  return button;
}

// Creates the button for "Managed Bookmarks", but does not position it.
- (void)createManagedBookmarksButton {
  if (managedBookmarksButton_.get()) {
    // The node's title might have changed if the user signed in or out.
    // Make sure it's up to date now.
    const BookmarkNode* node = managedBookmarkService_->managed_node();
    NSString* title = base::SysUTF16ToNSString(node->GetTitle());
    NSCell* cell = [managedBookmarksButton_ cell];
    [cell setTitle:title];

    // Its visibility may have changed too.
    [self setManagedBookmarksButtonVisibility];

    return;
  }

  NSCell* cell =
      [self cellForBookmarkNode:managedBookmarkService_->managed_node()];
  managedBookmarksButton_.reset([self createCustomBookmarkButtonForCell:cell]);
  [managedBookmarksButton_ setAction:@selector(openBookmarkFolderFromButton:)];
  view_id_util::SetID(managedBookmarksButton_.get(), VIEW_ID_MANAGED_BOOKMARKS);
  [buttonView_ addSubview:managedBookmarksButton_.get()];

  [self setManagedBookmarksButtonVisibility];
}

// Creates the button for "Supervised Bookmarks", but does not position it.
- (void)createSupervisedBookmarksButton {
  if (supervisedBookmarksButton_.get()) {
    // The button's already there, but its visibility may have changed.
    [self setSupervisedBookmarksButtonVisibility];
    return;
  }

  NSCell* cell =
      [self cellForBookmarkNode:managedBookmarkService_->supervised_node()];
  supervisedBookmarksButton_.reset(
      [self createCustomBookmarkButtonForCell:cell]);
  [supervisedBookmarksButton_
      setAction:@selector(openBookmarkFolderFromButton:)];
  view_id_util::SetID(supervisedBookmarksButton_.get(),
                      VIEW_ID_SUPERVISED_BOOKMARKS);
  [buttonView_ addSubview:supervisedBookmarksButton_.get()];

  [self setSupervisedBookmarksButtonVisibility];
}

// Creates the button for "Other Bookmarks", but does not position it.
- (void)createOtherBookmarksButton {
  // Can't create this until the model is loaded, but only need to
  // create it once.
  if (otherBookmarksButton_.get()) {
    [self setOtherBookmarksButtonVisibility];
    return;
  }

  NSCell* cell = [self cellForBookmarkNode:bookmarkModel_->other_node()];
  otherBookmarksButton_.reset([self createCustomBookmarkButtonForCell:cell]);
  // Peg at right; keep same height as bar.
  [otherBookmarksButton_ setAutoresizingMask:(NSViewMinXMargin)];
  [otherBookmarksButton_ setAction:@selector(openBookmarkFolderFromButton:)];
  view_id_util::SetID(otherBookmarksButton_.get(), VIEW_ID_OTHER_BOOKMARKS);
  [buttonView_ addSubview:otherBookmarksButton_.get()];

  [self setOtherBookmarksButtonVisibility];
}

// Creates the button for "Apps", but does not position it.
- (void)createAppsPageShortcutButton {
  // Can't create this until the model is loaded, but only need to
  // create it once.
  if (appsPageShortcutButton_.get()) {
    [self setAppsPageShortcutButtonVisibility];
    return;
  }

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  NSString* text = l10n_util::GetNSString(IDS_BOOKMARK_BAR_APPS_SHORTCUT_NAME);
  NSImage* image = rb.GetNativeImageNamed(
      IDR_BOOKMARK_BAR_APPS_SHORTCUT).ToNSImage();
  NSCell* cell = [self cellForCustomButtonWithText:text
                                             image:image];
  appsPageShortcutButton_.reset([self createCustomBookmarkButtonForCell:cell]);
  [[appsPageShortcutButton_ draggableButton] setActsOnMouseDown:NO];
  [appsPageShortcutButton_ setAction:@selector(openAppsPage:)];
  NSString* tooltip =
      l10n_util::GetNSString(IDS_BOOKMARK_BAR_APPS_SHORTCUT_TOOLTIP);
  [appsPageShortcutButton_ setToolTip:tooltip];
  [buttonView_ addSubview:appsPageShortcutButton_.get()];

  [self setAppsPageShortcutButtonVisibility];
}

- (void)createOffTheSideButton {
  offTheSideButton_.reset(
      [[BookmarkButton alloc] initWithFrame:NSMakeRect(586, 0, 20, 24)]);
  id offTheSideCell = [BookmarkButtonCell offTheSideButtonCell];
  [offTheSideCell setTag:kMaterialStandardButtonTypeWithLimitedClickFeedback];
  [offTheSideCell setImagePosition:NSImageOnly];

  [offTheSideCell setHighlightsBy:NSNoCellMask];
  [offTheSideCell setShowsBorderOnlyWhileMouseInside:YES];
  [offTheSideCell setBezelStyle:NSShadowlessSquareBezelStyle];
  [offTheSideButton_ setCell:offTheSideCell];
  [offTheSideButton_ setImage:[self offTheSideButtonImage:NO]];
  [offTheSideButton_ setButtonType:NSMomentaryLightButton];

  [offTheSideButton_ setTarget:self];
  [offTheSideButton_ setAction:@selector(openOffTheSideFolderFromButton:)];
  [offTheSideButton_ setDelegate:self];
  [[offTheSideButton_ draggableButton] setDraggable:NO];
  [[offTheSideButton_ draggableButton] setActsOnMouseDown:YES];
}

- (void)openAppsPage:(id)sender {
  WindowOpenDisposition disposition =
      ui::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);
  [self openURL:GURL(chrome::kChromeUIAppsURL) disposition:disposition];
  RecordBookmarkAppsPageOpen([self bookmarkLaunchLocation]);
}

// To avoid problems with sync, changes that may impact the current
// bookmark (e.g. deletion) make sure context menus are closed.  This
// prevents deleting a node which no longer exists.
- (void)cancelMenuTracking {
  [contextMenuController_ cancelTracking];
}

- (void)moveToState:(BookmarkBar::State)nextState
      withAnimation:(BOOL)animate {
  BOOL isAnimationRunning = [self isAnimationRunning];

  // No-op if the next state is the same as the "current" one, subject to the
  // following conditions:
  //  - no animation is running; or
  //  - an animation is running and |animate| is YES ([*] if it's NO, we'd want
  //    to cancel the animation and jump to the final state).
  if ((nextState == currentState_) && (!isAnimationRunning || animate))
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
    if (nextState == currentState_)
      return;
  }

  // Now update with the new state change.
  lastState_ = currentState_;
  currentState_ = nextState;
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
                     toState:currentState_];
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
  lastState_ = currentState_;
  isAnimationRunning_ = NO;

  // Notify our delegate.
  [delegate_ bookmarkBar:self
      didChangeFromState:oldState
                 toState:currentState_];

  // Update ourselves visually.
  [self updateVisibility];
}

// (Private)
- (void)stopCurrentAnimation {
  [[self controlledView] stopAnimation];
}

// Delegate method for |AnimatableView| (a superclass of
// |BookmarkBarToolbarView|).
- (void)animationDidEnd:(NSAnimation*)animation {
  [self finalizeState];
}

- (void)reconfigureBookmarkBar {
  [self setManagedBookmarksButtonVisibility];
  [self setSupervisedBookmarksButtonVisibility];
  [self redistributeButtonsOnBarAsNeeded];
  [self positionRightSideButtons];
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
- (void)unhighlightBookmark:(const BookmarkNode*)node {
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


// Adjust the horizontal width, x position and the visibility of the "For quick
// access" text field and "Import bookmarks..." button based on the current
// width of the containing |buttonView_| (which is affected by window width).
- (void)adjustNoItemContainerForMaxX:(CGFloat)maxViewX {
  if (![[buttonView_ noItemContainer] isHidden]) {
    // Reset initial frames for the two items, then adjust as necessary.
    NSTextField* noItemTextfield = [buttonView_ noItemTextfield];
    NSRect noItemsRect = originalNoItemsRect_;
    NSRect importBookmarksRect = originalImportBookmarksRect_;
    if (![appsPageShortcutButton_ isHidden]) {
      float width = NSWidth([appsPageShortcutButton_ frame]);
      noItemsRect.origin.x += width;
      importBookmarksRect.origin.x += width;
    }
    if (![managedBookmarksButton_ isHidden]) {
      float width = NSWidth([managedBookmarksButton_ frame]);
      noItemsRect.origin.x += width;
      importBookmarksRect.origin.x += width;
    }
    if (![supervisedBookmarksButton_ isHidden]) {
      float width = NSWidth([supervisedBookmarksButton_ frame]);
      noItemsRect.origin.x += width;
      importBookmarksRect.origin.x += width;
    }
    [noItemTextfield setFrame:noItemsRect];
    [noItemTextfield setHidden:NO];
    NSButton* importBookmarksButton = [buttonView_ importBookmarksButton];
    [importBookmarksButton setFrame:importBookmarksRect];
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
  CGFloat left = bookmarks::kBookmarkLeftMargin;
  NSRect buttonFrame = NSZeroRect;

  // Draw the apps bookmark if needed.
  if (![appsPageShortcutButton_ isHidden]) {
    left = NSMaxX([appsPageShortcutButton_ frame]) +
        bookmarks::kBookmarkHorizontalPadding;
  }

  // Draw the managed bookmarks folder if needed.
  if (![managedBookmarksButton_ isHidden]) {
    left = NSMaxX([managedBookmarksButton_ frame]) +
        bookmarks::kBookmarkHorizontalPadding;
  }

  // Draw the supervised bookmarks folder if needed.
  if (![supervisedBookmarksButton_ isHidden]) {
    left = NSMaxX([supervisedBookmarksButton_ frame]) +
        bookmarks::kBookmarkHorizontalPadding;
  }

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

- (CGFloat)buttonViewMaxXWithOffTheSideButtonIsVisible:(BOOL)visible {
  CGFloat maxViewX = NSMaxX([buttonView_ bounds]);
  // If necessary, pull in the width to account for the Other Bookmarks button.
  if ([self setOtherBookmarksButtonVisibility]) {
    maxViewX = [otherBookmarksButton_ frame].origin.x -
        bookmarks::kBookmarkRightMargin;
  }

  [self positionRightSideButtons];
  // If we're already overflowing, then we need to account for the chevron.
  if (visible) {
    maxViewX =
        [offTheSideButton_ frame].origin.x - bookmarks::kBookmarkRightMargin;
  }

  return maxViewX;
}

- (void)redistributeButtonsOnBarAsNeeded {
  const BookmarkNode* node = bookmarkModel_->bookmark_bar_node();
  NSInteger barCount = node->child_count();

  // Determine the current maximum extent of the visible buttons.
  [self positionRightSideButtons];
  BOOL offTheSideButtonVisible = (barCount > displayedButtonCount_);
  CGFloat maxViewX = [self buttonViewMaxXWithOffTheSideButtonIsVisible:
      offTheSideButtonVisible];

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
    // Account for the fact that the chevron might now be visible.
    if (!offTheSideButtonVisible) {
      offTheSideButtonVisible = YES;
      maxViewX = [self buttonViewMaxXWithOffTheSideButtonIsVisible:YES];
    }
  }

  // As a result of cutting, deleting and dragging, the bar may now have room
  // for more buttons.
  int xOffset;
  if (displayedButtonCount_ > 0) {
    xOffset = NSMaxX([self finalRectOfLastButton]);
  } else if (![managedBookmarksButton_ isHidden]) {
    xOffset = NSMaxX([managedBookmarksButton_ frame]) +
        bookmarks::kBookmarkHorizontalPadding;
  } else if (![supervisedBookmarksButton_ isHidden]) {
    xOffset = NSMaxX([supervisedBookmarksButton_ frame]) +
        bookmarks::kBookmarkHorizontalPadding;
  } else if (![appsPageShortcutButton_ isHidden]) {
    xOffset = NSMaxX([appsPageShortcutButton_ frame]) +
        bookmarks::kBookmarkHorizontalPadding;
  } else {
    xOffset = bookmarks::kBookmarkLeftMargin -
        bookmarks::kBookmarkHorizontalPadding;
  }
  for (int i = displayedButtonCount_; i < barCount; ++i) {
    const BookmarkNode* child = node->GetChild(i);
    BookmarkButton* button = [self buttonForNode:child xOffset:&xOffset];
    // If we're testing against the last possible button then account
    // for the chevron no longer needing to be shown.
    if (i == barCount - 1)
      maxViewX = [self buttonViewMaxXWithOffTheSideButtonIsVisible:NO];
    if (NSMaxX([button frame]) > maxViewX) {
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
    [self adjustNoItemContainerForMaxX:maxViewX];
}

#pragma mark Private Methods Exposed for Testing

- (BookmarkBarView*)buttonView {
  return buttonView_;
}

- (NSMutableArray*)buttons {
  return buttons_.get();
}

- (BOOL)offTheSideButtonIsHidden {
  return [offTheSideButton_ isHidden];
}

- (BOOL)appsPageShortcutButtonIsHidden {
  return [appsPageShortcutButton_ isHidden];
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
  [self stopPulsingBookmarkNode];
  for (BookmarkButton* button in buttons_.get()) {
    [button setDelegate:nil];
    [button removeFromSuperview];
  }
  [buttons_ removeAllObjects];
  displayedButtonCount_ = 0;

  // Make sure there are no stale pointers in the pasteboard.  This
  // can be important if a bookmark is deleted (via bookmark sync)
  // while in the middle of a drag.  The "drag completed" code
  // (e.g. [BookmarkBarView performDragOperationForBookmarkButton:]) is
  // careful enough to bail if there is no data found at "drop" time.
  [[NSPasteboard pasteboardWithName:NSDragPboard] clearContents];
}

// Return an autoreleased NSCell suitable for a bookmark button.
// TODO(jrg): move much of the cell config into the BookmarkButtonCell class.
- (BookmarkButtonCell*)cellForBookmarkNode:(const BookmarkNode*)node {
  BOOL darkTheme = [[[self view] window] hasDarkTheme];
  NSImage* image = node ? [self faviconForNode:node forADarkTheme:darkTheme]
                        : nil;
  BookmarkButtonCell* cell =
      [BookmarkButtonCell buttonCellForNode:node
                                       text:nil
                                      image:image
                             menuController:contextMenuController_];
  [cell setTag:kMaterialStandardButtonTypeWithLimitedClickFeedback];

  // Note: a quirk of setting a cell's text color is that it won't work
  // until the cell is associated with a button, so we can't theme the cell yet.

  return cell;
}

// Return an autoreleased NSCell suitable for a special button displayed on the
// bookmark bar that is not attached to any bookmark node.
// TODO(jrg): move much of the cell config into the BookmarkButtonCell class.
- (BookmarkButtonCell*)cellForCustomButtonWithText:(NSString*)text
                                             image:(NSImage*)image {
  BookmarkButtonCell* cell =
      [BookmarkButtonCell buttonCellWithText:text
                                       image:image
                              menuController:contextMenuController_];
  [cell setTag:kMaterialStandardButtonTypeWithLimitedClickFeedback];
  [cell setHighlightsBy:NSNoCellMask];

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
  if (!bookmarkModel_->loaded())
    return;
  [self updateTheme:[[[self view] window] themeProvider]];
  [self reconfigureBookmarkBar];
}

// Adapt appearance of buttons to the current theme. Called after
// theme changes, or when our view is added to the view hierarchy.
// Oddly, the view pings us instead of us pinging our view.  This is
// because our trigger is an [NSView viewWillMoveToWindow:], which the
// controller doesn't normally know about.  Otherwise we don't have
// access to the theme before we know what window we will be on.
- (void)updateTheme:(const ui::ThemeProvider*)themeProvider {
  if (!themeProvider)
    return;
  NSColor* color =
      themeProvider->GetNSColor(ThemeProperties::COLOR_BOOKMARK_TEXT);
  for (BookmarkButton* button in buttons_.get()) {
    BookmarkButtonCell* cell = [button cell];
    [cell setTextColor:color];
  }
  [[managedBookmarksButton_ cell] setTextColor:color];
  [[supervisedBookmarksButton_ cell] setTextColor:color];
  [[otherBookmarksButton_ cell] setTextColor:color];
  [[appsPageShortcutButton_ cell] setTextColor:color];
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
        if (![hitView isDescendantOf:[self view]] ||
            hitView == buttonView_.get())
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
        // Ignore all modifiers like Cmd - keyboard shortcuts should not work
        // while a bookmark folder window (essentially a menu) is open.
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

  if (!managedBookmarkService_->CanBeEditedByUser(destParent))
    return NO;
  if (!managedBookmarkService_->CanBeEditedByUser(sourceNode))
    copy = YES;

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
    CGFloat left;
    if (![supervisedBookmarksButton_ isHidden]) {
      left = NSMaxX([supervisedBookmarksButton_ frame]) +
             bookmarks::kBookmarkHorizontalPadding;
    } else if (![managedBookmarksButton_ isHidden]) {
      left = NSMaxX([managedBookmarksButton_ frame]) +
             bookmarks::kBookmarkHorizontalPadding;
    } else if (![appsPageShortcutButton_ isHidden]) {
      left = NSMaxX([appsPageShortcutButton_ frame]) +
             bookmarks::kBookmarkHorizontalPadding;
    } else {
      left = bookmarks::kBookmarkLeftMargin;
    }
    CGFloat paddingWidth = bookmarks::kDefaultBookmarkWidth;
    BookmarkButton* draggedButton = [BookmarkButton draggedButton];
    if (draggedButton) {
      paddingWidth = std::min(bookmarks::kDefaultBookmarkWidth,
                              NSWidth([draggedButton frame]));
    }
    // Put all the buttons where they belong, with all buttons to the right
    // of the insertion point shuffling right to make space for it.
    [NSAnimationContext beginGrouping];
    [[NSAnimationContext currentContext]
        setDuration:kDragAndDropAnimationDuration];
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
    [NSAnimationContext endGrouping];
  }
}

// Put all visible bookmark bar buttons in their normal locations, either with
// or without animation according to the |animate| flag.
// This is generally useful, so is called from various places internally.
- (void)resetAllButtonPositionsWithAnimation:(BOOL)animate {

  // Position the apps bookmark if needed.
  CGFloat left = bookmarks::kBookmarkLeftMargin;
  if (![appsPageShortcutButton_ isHidden]) {
    int xOffset = bookmarks::kBookmarkLeftMargin -
            bookmarks::kBookmarkHorizontalPadding;
    NSRect frame =
        [self frameForBookmarkButtonFromCell:[appsPageShortcutButton_ cell]
                                     xOffset:&xOffset];
    [appsPageShortcutButton_ setFrame:frame];
    left = xOffset + bookmarks::kBookmarkHorizontalPadding;
  }

  // Position the managed bookmarks folder if needed.
  if (![managedBookmarksButton_ isHidden]) {
    int xOffset = left;
    NSRect frame =
        [self frameForBookmarkButtonFromCell:[managedBookmarksButton_ cell]
                                     xOffset:&xOffset];
    [managedBookmarksButton_ setFrame:frame];
    left = xOffset + bookmarks::kBookmarkHorizontalPadding;
  }

  // Position the supervised bookmarks folder if needed.
  if (![supervisedBookmarksButton_ isHidden]) {
    int xOffset = left;
    NSRect frame =
        [self frameForBookmarkButtonFromCell:[supervisedBookmarksButton_ cell]
                                     xOffset:&xOffset];
    [supervisedBookmarksButton_ setFrame:frame];
    left = xOffset + bookmarks::kBookmarkHorizontalPadding;
  }

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
  if (!model->loaded())
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
  [self createAppsPageShortcutButton];
  [self createManagedBookmarksButton];
  [self createSupervisedBookmarksButton];
  [self addNodesToButtonList:node];
  [self createOtherBookmarksButton];
  [self updateTheme:[[[self view] window] themeProvider]];
  [self positionRightSideButtons];
  [self addButtonsToView];
  [self configureOffTheSideButtonContentsAndVisibility];
  [self reconfigureBookmarkBar];
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
      BOOL darkTheme = [[[self view] window] hasDarkTheme];
      NSImage* theImage = [self faviconForNode:node forADarkTheme:darkTheme];
      [[button cell] setBookmarkCellText:[button title]
                                   image:theImage];
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
  return barIsEnabled_ && (currentState_ == BookmarkBar::SHOW ||
                           currentState_ == BookmarkBar::DETACHED ||
                           lastState_ == BookmarkBar::SHOW ||
                           lastState_ == BookmarkBar::DETACHED);
}

// (BookmarkBarState protocol)
- (BOOL)isInState:(BookmarkBar::State)state {
  return currentState_ == state && ![self isAnimationRunning];
}

// (BookmarkBarState protocol)
- (BOOL)isAnimatingToState:(BookmarkBar::State)state {
  return currentState_ == state && [self isAnimationRunning];
}

// (BookmarkBarState protocol)
- (BOOL)isAnimatingFromState:(BookmarkBar::State)state {
  return lastState_ == state && [self isAnimationRunning];
}

// (BookmarkBarState protocol)
- (BOOL)isAnimatingFromState:(BookmarkBar::State)fromState
                     toState:(BookmarkBar::State)toState {
  return lastState_ == fromState &&
         currentState_ == toState &&
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
        [[self controlledView] currentAnimationProgress]);
  }
  if ([self isAnimatingFromState:BookmarkBar::DETACHED]) {
    return static_cast<CGFloat>(
        1 - [[self controlledView] currentAnimationProgress]);
  }
  return 0;
}

#pragma mark BookmarkBarToolbarViewController Protocol

- (int)currentTabContentsHeight {
  BrowserWindowController* browserController =
      [BrowserWindowController browserWindowControllerForView:[self view]];
  return NSHeight([[browserController tabContentArea] frame]);
}

- (Profile*)profile {
  return browser_->profile();
}

#pragma mark BookmarkButtonDelegate Protocol

- (NSPasteboardItem*)pasteboardItemForDragOfButton:(BookmarkButton*)button {
  return [[self folderTarget] pasteboardItemForDragOfButton:button];
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
  // button while the BookmarkBarFolderController DOES automatically open
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
  const BookmarkNode* node = [sender bookmarkNode];
  if (node && node->is_folder()) {
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
    if (node)
      bookmarkModel_->Remove(node);
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

  // Grab the parent button under to make sure that the highlighting that was
  // applied while revealing the menu is turned off.
  BookmarkButton* parentButton = [folderController_ parentButton];
  [folderController_ close];
  [parentButton setNeedsDisplay:YES];
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
        dataForType:ui::ClipboardUtil::UTIForPasteboardType(
                        kBookmarkButtonDragType)];
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
  if (dragData.ReadFromClipboard(ui::CLIPBOARD_TYPE_DRAG)) {
    std::vector<const BookmarkNode*> nodes(
        dragData.GetNodes(bookmarkModel_, browser_->profile()->GetPath()));
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
  CGFloat halfHorizontalPadding = 0.5 * bookmarks::kBookmarkHorizontalPadding;
  int destIndex = [self indexForDragToPoint:point];
  int numButtons = displayedButtonCount_;

  CGFloat leftmostX;
  if (![supervisedBookmarksButton_ isHidden]) {
    leftmostX =
        NSMaxX([supervisedBookmarksButton_ frame]) + halfHorizontalPadding;
  } else if (![managedBookmarksButton_ isHidden]) {
    leftmostX = NSMaxX([managedBookmarksButton_ frame]) + halfHorizontalPadding;
  } else if (![appsPageShortcutButton_ isHidden]) {
    leftmostX = NSMaxX([appsPageShortcutButton_ frame]) + halfHorizontalPadding;
  } else {
    leftmostX = bookmarks::kBookmarkLeftMargin - halfHorizontalPadding;
  }

  // If it's a drop strictly between existing buttons ...
  if (destIndex == 0) {
    x = leftmostX;
  } else if (destIndex > 0 && destIndex < numButtons) {
    // ... put the indicator right between the buttons.
    BookmarkButton* button =
        [buttons_ objectAtIndex:static_cast<NSUInteger>(destIndex-1)];
    DCHECK(button);
    NSRect buttonFrame = [button frame];
    x = NSMaxX(buttonFrame) + halfHorizontalPadding;

    // If it's a drop at the end (past the last button, if there are any) ...
  } else if (destIndex == numButtons) {
    // and if it's past the last button ...
    if (numButtons > 0) {
      // ... find the last button, and put the indicator to its right.
      BookmarkButton* button =
          [buttons_ objectAtIndex:static_cast<NSUInteger>(destIndex - 1)];
      DCHECK(button);
      x = NSMaxX([button frame]) + halfHorizontalPadding;

      // Otherwise, put it right at the beginning.
    } else {
      x = leftmostX;
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
    [browserController lockToolbarVisibilityForOwner:child withAnimation:NO];
  }
}

- (void)childFolderWillClose:(id<BookmarkButtonControllerProtocol>)child {
  // Release bar visibility, allowing the overlay to close if in fullscreen
  // mode.
  BrowserWindowController* browserController =
      [BrowserWindowController browserWindowControllerForView:[self view]];
  [browserController releaseToolbarVisibilityForOwner:child withAnimation:NO];
}

// Add a new folder controller as triggered by the given folder button.
- (void)addNewFolderControllerWithParentButton:(BookmarkButton*)parentButton {

  // If doing a close/open, make sure the fullscreen chrome doesn't
  // have a chance to begin animating away in the middle of things.
  BrowserWindowController* browserController =
      [BrowserWindowController browserWindowControllerForView:[self view]];
  // Confirm we're not re-locking with ourself as an owner before locking.
  DCHECK([browserController isToolbarVisibilityLockedForOwner:self] == NO);
  [browserController lockToolbarVisibilityForOwner:self withAnimation:NO];

  if (folderController_)
    [self closeAllBookmarkFolders];

  // Folder controller, like many window controllers, owns itself.
  folderController_ =
      [[BookmarkBarFolderController alloc]
          initWithParentButton:parentButton
              parentController:nil
                 barController:self
                       profile:browser_->profile()];
  [folderController_ showWindow:self];

  // Only BookmarkBarController has this; the
  // BookmarkBarFolderController does not.
  [self watchForExitEvent:YES];

  // No longer need to hold the lock; the folderController_ now owns it.
  [browserController releaseToolbarVisibilityForOwner:self withAnimation:NO];
}

- (void)openAll:(const BookmarkNode*)node
    disposition:(WindowOpenDisposition)disposition {
  [self closeFolderAndStopTrackingMenus];
  chrome::OpenAll([[self view] window], browser_, node, disposition,
                  browser_->profile());
}

- (void)addButtonForNode:(const BookmarkNode*)node
                 atIndex:(NSInteger)buttonIndex {
  int newOffset =
      bookmarks::kBookmarkLeftMargin - bookmarks::kBookmarkHorizontalPadding;
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

  if (!managedBookmarkService_->CanBeEditedByUser(destParent))
    return NO;

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
    [folderController_ removeButton:index animate:animate];
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

@end
