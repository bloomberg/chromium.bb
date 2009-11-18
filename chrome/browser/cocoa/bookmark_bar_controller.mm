// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util_mac.h"
#include "app/resource_bundle.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#import "chrome/browser/cocoa/background_gradient_view.h"
#import "chrome/browser/cocoa/bookmark_bar_bridge.h"
#import "chrome/browser/cocoa/bookmark_bar_constants.h"
#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#import "chrome/browser/cocoa/bookmark_bar_toolbar_view.h"
#import "chrome/browser/cocoa/bookmark_bar_view.h"
#import "chrome/browser/cocoa/bookmark_button.h"
#import "chrome/browser/cocoa/bookmark_button_cell.h"
#import "chrome/browser/cocoa/bookmark_editor_controller.h"
#import "chrome/browser/cocoa/bookmark_menu.h"
#import "chrome/browser/cocoa/bookmark_menu_cocoa_controller.h"
#import "chrome/browser/cocoa/bookmark_name_folder_controller.h"
#import "chrome/browser/cocoa/event_utils.h"
#import "chrome/browser/cocoa/menu_button.h"
#import "chrome/browser/cocoa/toolbar_controller.h"
#import "chrome/browser/cocoa/view_resizer.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
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

- (void)addNode:(const BookmarkNode*)child toMenu:(NSMenu*)menu;
- (void)addFolderNode:(const BookmarkNode*)node toMenu:(NSMenu*)menu;
- (void)tagEmptyMenu:(NSMenu*)menu;
- (void)clearMenuTagMap;
- (int)preferredHeight;
- (void)addNonBookmarkButtonsToView;
- (void)addButtonsToView;
- (void)resizeButtons;
- (void)centerNoItemsLabel;
- (NSImage*)getFavIconForNode:(const BookmarkNode*)node;
- (void)setNodeForBarMenu;
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

  bridge_.reset(NULL);
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
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
                      (frame.size.width + bookmarks::kBookmarkHorizontalPadding));
    [offTheSideButton_ setFrame:frame];
  }
}

// Check if we should enable or disable the off-the-side chevron.
// Assumes that buttons which don't fit in the parent view are removed
// from it.
//
// TODO(jrg): when we are smarter about creating buttons (e.g. don't
// bother creating buttons which aren't visible), we'll have to be
// smarter here too.
- (void)showOrHideOffTheSideButton {
  // Then determine if we'll hide or show it.
  NSButton* button = [buttons_ lastObject];
  if (button && ![button superview]) {
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
  [self resizeButtons];
  [self positionOffTheSideButton];
  [self addButtonsToView];
  [self showOrHideOffTheSideButton];
  [self centerNoItemsLabel];
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
  [[self view] setHidden:([self isVisible] ? NO : YES)];

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
    // The toolbar uncompresses immediately at the beginning (otherwise the
    // slide looks wrong, since we slide into the bottom of stuff in the
    // toolbar). Do this only if we're at the beginning height since we may
    // enter this mid-animation.
    if (NSHeight([[self view] frame]) == bookmarks::kBookmarkBarHeight) {
      [resizeDelegate_ resizeView:[self view]
                        newHeight:(bookmarks::kBookmarkBarHeight -
                                      kBookmarkBarOverlap)];
    }
    [[self backgroundGradientView] setShowsDivider:YES];
    [[self view] setHidden:NO];
    AnimatableView* view = [self animatableView];
    [view animateToNewHeight:0
                    duration:kBookmarkBarAnimationDuration];
  } else if ([self isAnimatingFromState:bookmarks::kShowingState
                                toState:bookmarks::kDetachedState]) {
    // The toolbar uncompresses immediately at the beginning (otherwise the
    // slide looks wrong, since we slide into the bottom of stuff in the
    // toolbar). Do this only if we're at the beginning height since we may
    // enter this mid-animation.
    if (NSHeight([[self view] frame]) == bookmarks::kBookmarkBarHeight) {
      [resizeDelegate_ resizeView:[self view]
                        newHeight:(bookmarks::kBookmarkBarHeight -
                                      kBookmarkBarOverlap)];
    }
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
  barIsEnabled_ = enabled;
  [self updateVisibility];
}

- (CGFloat)getDesiredToolbarHeightCompression {
  // Some special cases....
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

- (BOOL)dragButton:(BookmarkButton*)sourceButton to:(NSPoint)point {
  DCHECK([sourceButton isKindOfClass:[BookmarkButton class]]);

  void* pointer = [[[sourceButton cell] representedObject] pointerValue];
  const BookmarkNode* sourceNode = static_cast<const BookmarkNode*>(pointer);
  DCHECK(sourceNode);

  // Identify which buttons we are between.  For now, assume a button
  // location is at the center point of its view, and that an exact
  // match means "place before".
  // TODO(jrg): revisit position info based on UI team feedback.
  // dropLocation is in bar local coordinates.
  NSPoint dropLocation = [[self view] convertPoint:point
                                          fromView:[[[self view] window]
                                                     contentView]];
  NSButton* buttonToTheRightOfDraggedButton = nil;
  for (NSButton* button in buttons_.get()) {
    CGFloat midpoint = NSMidX([button frame]);
    if (dropLocation.x <= midpoint) {
      buttonToTheRightOfDraggedButton = button;
      break;
    }
  }
  if (buttonToTheRightOfDraggedButton) {
    pointer = [[[buttonToTheRightOfDraggedButton cell]
                 representedObject] pointerValue];
    const BookmarkNode* afterNode = static_cast<const BookmarkNode*>(pointer);
    bookmarkModel_->Move(sourceNode, sourceNode->GetParent(),
                         afterNode->GetParent()->IndexOfChild(afterNode));
  } else {
    // If nothing is to my right I am at the end!
    bookmarkModel_->Move(sourceNode, sourceNode->GetParent(),
                         sourceNode->GetParent()->GetChildCount());
  }

  // Movement of a node triggers observers (like us) to rebuild the
  // bar so we don't have to do so explicitly.

  return YES;
}

- (int)currentTabContentsHeight {
  return browser_->GetSelectedTabContents() ?
      browser_->GetSelectedTabContents()->view()->GetContainerSize().height() :
      0;
}

- (ThemeProvider*)themeProvider {
  return browser_->profile()->GetThemeProvider();
}

- (BookmarkNode*)nodeFromButton:(id)button {
  NSCell* cell = [button cell];
  BookmarkNode* node = static_cast<BookmarkNode*>(
      [[cell representedObject] pointerValue]);
  DCHECK(node);
  return node;
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
  BookmarkNode* node = [self nodeFromButton:sender];
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

// Called from a Folder bookmark button.
- (IBAction)openFolderMenuFromButton:(id)sender {
  NSMenu* menu = [self menuForFolderNode:[self nodeFromButton:sender]];
  if (menu) {
    [NSMenu popUpContextMenu:menu
                   withEvent:[NSApp currentEvent]
                     forView:sender];
  }
}

// Rebuild the off-the-side menu, taking into account which buttons are
// displayed.
// TODO(jrg,viettrungluu): only (re)build the menu when necessary.
// TODO(jrg): if we get smarter such that we don't even bother
//   creating buttons which aren't visible, we'll need to be smarter
//   here.
- (void)buildOffTheSideMenu {
  NSMenu* menu = [self offTheSideMenu];
  DCHECK(menu);

  // Remove old menu items (backwards order is as good as any); leave the
  // blank one at position 0 (see menu_button.h).
  for (NSInteger i = [menu numberOfItems] - 1; i >= 1 ; i--)
    [menu removeItemAtIndex:i];

  // Add items corresponding to buttons which aren't displayed.
  for (NSButton* button in buttons_.get()) {
    if (![button superview])
      [self addNode:[self nodeFromButton:button] toMenu:menu];
  }
}

// Get the off-the-side menu.
- (NSMenu*)offTheSideMenu {
  return [offTheSideButton_ attachedMenu];
}

// Called by any menus which have set us as their delegate (right now just the
// off-the-side menu?).
- (void)menuNeedsUpdate:(NSMenu*)menu {
  if (menu == [self offTheSideMenu])
    [self buildOffTheSideMenu];
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
  const std::string spec = node->GetURL().spec();
  NSString* url = base::SysUTF8ToNSString(spec);
  NSString* title = base::SysWideToNSString(node->GetTitle());
  NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
  [pasteboard declareURLPasteboardWithAdditionalTypes:[NSArray array]
                                                owner:nil];
  [pasteboard setDataForURL:url title:title];
}

- (IBAction)deleteBookmark:(id)sender {
  BookmarkNode* node = [self nodeFromMenuItem:sender];
  bookmarkModel_->Remove(node->GetParent(),
                         node->GetParent()->IndexOfChild(node));
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
}

// Return an autoreleased NSCell suitable for a bookmark button.
// TODO(jrg): move much of the cell config into the BookmarkButtonCell class.
- (NSCell*)cellForBookmarkNode:(const BookmarkNode*)node {
  NSString* title = base::SysWideToNSString(node->GetTitle());
  BookmarkButtonCell* cell =
      [[[BookmarkButtonCell alloc] initTextCell:nil] autorelease];
  DCHECK(cell);
  [cell setRepresentedObject:[NSValue valueWithPointer:node]];

  NSImage* image = [self getFavIconForNode:node];
  [cell setBookmarkCellText:title image:image];
  if (node->is_folder())
    [cell setMenu:buttonFolderContextMenu_];
  else
    [cell setMenu:buttonContextMenu_];
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

// Create buttons for all items in the bookmark node tree.
//
// TODO(jrg): write a "build bar" so there is a nice spot for things
// like the contextual menu which is invoked when not over a
// bookmark.  On Safari that menu has a "new folder" option.
- (void)addNodesToButtonList:(const BookmarkNode*)node {
  BOOL hidden = (node->GetChildCount() == 0) ? NO : YES;
  [[buttonView_ noItemTextfield] setHidden:hidden];

  int xOffset = 0;
  for (int i = 0; i < node->GetChildCount(); i++) {
    const BookmarkNode* child = node->GetChild(i);

    NSCell* cell = [self cellForBookmarkNode:child];
    NSRect frame = [self frameForBookmarkButtonFromCell:cell xOffset:&xOffset];
    NSButton* button = [[[BookmarkButton alloc] initWithFrame:frame]
                         autorelease];
    DCHECK(button);
    [buttons_ addObject:button];

    // [NSButton setCell:] warns to NOT use setCell: other than in the
    // initializer of a control.  However, we are using a basic
    // NSButton whose initializer does not take an NSCell as an
    // object.  To honor the assumed semantics, we do nothing with
    // NSButton between alloc/init and setCell:.
    [button setCell:cell];

    if (child->is_folder()) {
      [button setTarget:self];
      [button setAction:@selector(openFolderMenuFromButton:)];
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
  for (NSButton* button in buttons_.get()) {
    superview = [button superview];
    if (NSMaxX([button frame]) <= NSMinX([offTheSideButton_ frame])) {
      if (!superview)
        [buttonView_ addSubview:button];
    } else {
      if (superview)
        [button removeFromSuperview];
    }
  }
}

// Helper for resizeButtons to resize buttons based on a new parent
// view height.
- (void)resizeButtonsInArray:(NSArray*)array {
  NSRect parentBounds = [buttonView_ bounds];
  CGFloat height = (bookmarks::kBookmarkButtonHeight -
                    (bookmarks::kBookmarkVerticalPadding*2));
  for (NSButton* button in array) {
    NSRect frame = [button frame];
    frame.size.height = height;
    [button setFrame:frame];
  }
}

// Resize our buttons; the parent view height may have changed.  This
// applies to all bookmarks, the chevron, and the "Other Bookmarks"
- (void)resizeButtons {
  [self resizeButtonsInArray:buttons_.get()];
  NSMutableArray* array = [NSMutableArray arrayWithObject:offTheSideButton_];
  // We must handle resize before the bookmarks are loaded.  If not loaded,
  // we have no otherBookmarksButton_ yet.
  if (otherBookmarksButton_.get())
    [array addObject:otherBookmarksButton_.get()];
  [self resizeButtonsInArray:array];
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
  [button setTarget:self];
  [button setAction:@selector(openFolderMenuFromButton:)];
  [buttonView_ addSubview:button];

  // Now that it's here, move the chevron over.
  [self positionOffTheSideButton];

  // Force it to be the right size, right now.
  [self resizeButtons];
}

// TODO(jrg): for now this is brute force.
- (void)loaded:(BookmarkModel*)model {
  DCHECK(model == bookmarkModel_);
  if (!model->IsLoaded())
    return;
  // Else brute force nuke and build.
  const BookmarkNode* node = model->GetBookmarkBarNode();
  [self clearBookmarkBar];
  [self addNodesToButtonList:node];
  [self createOtherBookmarksButton];
  [self resizeButtons];
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
  for (NSButton* button in buttons_.get()) {
    BookmarkButtonCell* cell = [button cell];
    void* pointer = [[cell representedObject] pointerValue];
    const BookmarkNode* cellnode = static_cast<const BookmarkNode*>(pointer);
    if (cellnode == node) {
      [cell setBookmarkCellText:[cell title]
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

  if (animate) {
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

// Delegate method for |AnimatableView| (a superclass of
// |BookmarkBarToolbarView|).
- (void)animationDidEnd:(NSAnimation*)animation {
  [self finalizeVisualState];
}

// (BookmarkBarState protocol)
- (BOOL)isVisible {
  return (barIsEnabled_ && (visualState_ == bookmarks::kShowingState ||
                            visualState_ == bookmarks::kDetachedState)) ?
      YES : NO;
}

// (BookmarkBarState protocol)
- (BOOL)isAnimationRunning {
  return (lastVisualState_ == bookmarks::kInvalidState) ? NO : YES;
}

// (BookmarkBarState protocol)
- (BOOL)isInState:(bookmarks::VisualState)state {
  return (visualState_ == state &&
          lastVisualState_ == bookmarks::kInvalidState) ? YES : NO;
}

// (BookmarkBarState protocol)
- (BOOL)isAnimatingToState:(bookmarks::VisualState)state {
  return (visualState_ == state &&
          lastVisualState_ != bookmarks::kInvalidState) ? YES : NO;
}

// (BookmarkBarState protocol)
- (BOOL)isAnimatingFromState:(bookmarks::VisualState)state {
  return (lastVisualState_ == state) ? YES : NO;
}

// (BookmarkBarState protocol)
- (BOOL)isAnimatingFromState:(bookmarks::VisualState)fromState
                     toState:(bookmarks::VisualState)toState {
  return (lastVisualState_ == fromState && visualState_ == toState) ? YES : NO;
}

// (BookmarkBarState protocol)
- (BOOL)isAnimatingBetweenState:(bookmarks::VisualState)fromState
                       andState:(bookmarks::VisualState)toState {
  return ((lastVisualState_ == fromState && visualState_ == toState) ||
          (visualState_ == fromState && lastVisualState_ == toState)) ?
      YES : NO;
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

@end
