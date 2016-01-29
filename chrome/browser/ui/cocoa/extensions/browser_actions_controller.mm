// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/browser_actions_controller.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/macros.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/extensions/extension_message_bubble_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/extensions/browser_action_button.h"
#import "chrome/browser/ui/cocoa/extensions/browser_actions_container_view.h"
#import "chrome/browser/ui/cocoa/extensions/extension_message_bubble_bridge.h"
#import "chrome/browser/ui/cocoa/extensions/extension_popup_controller.h"
#import "chrome/browser/ui/cocoa/extensions/toolbar_actions_bar_bubble_mac.h"
#import "chrome/browser/ui/cocoa/image_button_cell.h"
#import "chrome/browser/ui/cocoa/menu_button.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/extensions/extension_toolbar_icon_surfacing_bubble_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_delegate.h"
#include "grit/theme_resources.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMNSAnimation+Duration.h"
#include "ui/base/cocoa/appkit_utils.h"

NSString* const kBrowserActionVisibilityChangedNotification =
    @"BrowserActionVisibilityChangedNotification";

namespace {

const CGFloat kAnimationDuration = 0.2;

const CGFloat kChevronWidth = 18;

// How far to inset from the bottom of the view to get the top border
// of the popup 2px below the bottom of the Omnibox.
const CGFloat kBrowserActionBubbleYOffset = 3.0;

}  // namespace

@interface BrowserActionsController(Private)

// Creates and adds a view for the given |action| at |index|.
- (void)addViewForAction:(ToolbarActionViewController*)action
               withIndex:(NSUInteger)index;

// Removes the view for the given |action| from the ccontainer.
- (void)removeViewForAction:(ToolbarActionViewController*)action;

// Removes views for all actions.
- (void)removeAllViews;

// Redraws the BrowserActionsContainerView and updates the button order to match
// the order in the ToolbarActionsBar.
- (void)redraw;

// Resizes the container to the specified |width|, and animates according to
// the ToolbarActionsBar.
- (void)resizeContainerToWidth:(CGFloat)width;

// Sets the container to be either hidden or visible based on whether there are
// any actions to show.
// Returns whether the container is visible.
- (BOOL)updateContainerVisibility;

// During container resizing, buttons become more transparent as they are pushed
// off the screen. This method updates each button's opacity determined by the
// position of the button.
- (void)updateButtonOpacity;

// When the container is resizing, there's a chance that the buttons' frames
// need to be adjusted (for instance, if an action is added to the left, the
// frames of the actions to the right should gradually move right in the
// container). Adjust the frames accordingly.
- (void)updateButtonPositions;

// Returns the existing button associated with the given id; nil if it cannot be
// found.
- (BrowserActionButton*)buttonForId:(const std::string&)id;

// Returns the button at the given index. This is just a wrapper around
// [NSArray objectAtIndex:], since that technically defaults to returning ids
// (and can cause compile errors).
- (BrowserActionButton*)buttonAtIndex:(NSUInteger)index;

// Notification handlers for events registered by the class.

// Updates each button's opacity, the cursor rects and chevron position.
- (void)containerFrameChanged:(NSNotification*)notification;

// Hides the chevron and unhides every hidden button so that dragging the
// container out smoothly shows the Browser Action buttons.
- (void)containerDragStart:(NSNotification*)notification;

// Determines which buttons need to be hidden based on the new size, hides them
// and updates the chevron overflow menu. Also fires a notification to let the
// toolbar know that the drag has finished.
- (void)containerDragFinished:(NSNotification*)notification;

// Shows the toolbar info bubble, if it should be displayed.
- (void)containerMouseEntered:(NSNotification*)notification;

// Notifies the controlling ToolbarActionsBar that any running animation has
// ended.
- (void)containerAnimationEnded:(NSNotification*)notification;

// Processes a key event from the container.
- (void)containerKeyEvent:(NSNotification*)notification;

// Adjusts the position of the surrounding action buttons depending on where the
// button is within the container.
- (void)actionButtonDragging:(NSNotification*)notification;

// Updates the position of the Browser Actions within the container. This fires
// when _any_ Browser Action button is done dragging to keep all open windows in
// sync visually.
- (void)actionButtonDragFinished:(NSNotification*)notification;

// Returns the frame that the button with the given |index| should have.
- (NSRect)frameForIndex:(NSUInteger)index;

// Returns the popup point for the given |view| with |bounds|.
- (NSPoint)popupPointForView:(NSView*)view
                  withBounds:(NSRect)bounds;

// Moves the given button both visually and within the toolbar model to the
// specified index.
- (void)moveButton:(BrowserActionButton*)button
           toIndex:(NSUInteger)index;

// Handles clicks for BrowserActionButtons.
- (BOOL)browserActionClicked:(BrowserActionButton*)button;

// The reason |frame| is specified in these chevron functions is because the
// container may be animating and the end frame of the animation should be
// passed instead of the current frame (which may be off and cause the chevron
// to jump at the end of its animation).

// Shows the overflow chevron button depending on whether there are any hidden
// extensions within the frame given.
- (void)showChevronIfNecessaryInFrame:(NSRect)frame;

// Moves the chevron to its correct position within |frame|.
- (void)updateChevronPositionInFrame:(NSRect)frame;

// Shows or hides the chevron in the given |frame|.
- (void)setChevronHidden:(BOOL)hidden
                 inFrame:(NSRect)frame;

// Handles when a menu item within the chevron overflow menu is selected.
- (void)chevronItemSelected:(id)menuItem;

// Updates the container's grippy cursor based on the number of hidden buttons.
- (void)updateGrippyCursors;

// Returns the associated ToolbarController.
- (ToolbarController*)toolbarController;

// Creates a message bubble anchored to the given |anchorAction|, or the app
// menu if no |anchorAction| is null.
- (ToolbarActionsBarBubbleMac*)createMessageBubble:
    (scoped_ptr<ToolbarActionsBarBubbleDelegate>)delegate
    anchorToSelf:(BOOL)anchorToSelf;

// Called when the window for the active bubble is closing, and sets the active
// bubble to nil.
- (void)bubbleWindowClosing:(NSNotification*)notification;

// Sets the current focused view. Should only be used for the overflow
// container.
- (void)setFocusedViewIndex:(NSInteger)index;

@end

namespace {

// A bridge between the ToolbarActionsBar and the BrowserActionsController.
class ToolbarActionsBarBridge : public ToolbarActionsBarDelegate {
 public:
  explicit ToolbarActionsBarBridge(BrowserActionsController* controller);
  ~ToolbarActionsBarBridge() override;

  BrowserActionsController* controller_for_test() { return controller_; }

 private:
  // ToolbarActionsBarDelegate:
  void AddViewForAction(ToolbarActionViewController* action,
                        size_t index) override;
  void RemoveViewForAction(ToolbarActionViewController* action) override;
  void RemoveAllViews() override;
  void Redraw(bool order_changed) override;
  void ResizeAndAnimate(gfx::Tween::Type tween_type,
                        int target_width,
                        bool suppress_chevron) override;
  void SetChevronVisibility(bool chevron_visible) override;
  int GetWidth(GetWidthTime get_width_time) const override;
  bool IsAnimating() const override;
  void StopAnimating() override;
  int GetChevronWidth() const override;
  void ShowExtensionMessageBubble(
      scoped_ptr<extensions::ExtensionMessageBubbleController> controller,
      ToolbarActionViewController* anchor_action) override;

  // The owning BrowserActionsController; weak.
  BrowserActionsController* controller_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarActionsBarBridge);
};

ToolbarActionsBarBridge::ToolbarActionsBarBridge(
    BrowserActionsController* controller)
    : controller_(controller) {
}

ToolbarActionsBarBridge::~ToolbarActionsBarBridge() {
}

void ToolbarActionsBarBridge::AddViewForAction(
    ToolbarActionViewController* action,
    size_t index) {
  [controller_ addViewForAction:action
                      withIndex:index];
}

void ToolbarActionsBarBridge::RemoveViewForAction(
    ToolbarActionViewController* action) {
  [controller_ removeViewForAction:action];
}

void ToolbarActionsBarBridge::RemoveAllViews() {
  [controller_ removeAllViews];
}

void ToolbarActionsBarBridge::Redraw(bool order_changed) {
  [controller_ redraw];
}

void ToolbarActionsBarBridge::ResizeAndAnimate(gfx::Tween::Type tween_type,
                                               int target_width,
                                               bool suppress_chevron) {
  [controller_ resizeContainerToWidth:target_width];
}

void ToolbarActionsBarBridge::SetChevronVisibility(bool chevron_visible) {
  [controller_ setChevronHidden:!chevron_visible
                        inFrame:[[controller_ containerView] frame]];
}

int ToolbarActionsBarBridge::GetWidth(GetWidthTime get_width_time) const {
  NSRect frame =
      get_width_time == ToolbarActionsBarDelegate::GET_WIDTH_AFTER_ANIMATION
          ? [[controller_ containerView] animationEndFrame]
          : [[controller_ containerView] frame];
  return NSWidth(frame);
}

bool ToolbarActionsBarBridge::IsAnimating() const {
  return [[controller_ containerView] isAnimating];
}

void ToolbarActionsBarBridge::StopAnimating() {
  // Unfortunately, animating the browser actions container affects neighboring
  // views (like the omnibox), which could also be animating. Because of this,
  // instead of just ending the animation, the cleanest way to terminate is to
  // "animate" to the current frame.
  [controller_ resizeContainerToWidth:
      NSWidth([[controller_ containerView] frame])];
}

int ToolbarActionsBarBridge::GetChevronWidth() const {
  return kChevronWidth;
}

void ToolbarActionsBarBridge::ShowExtensionMessageBubble(
    scoped_ptr<extensions::ExtensionMessageBubbleController> bubble_controller,
    ToolbarActionViewController* anchor_action) {
  // This goop is a by-product of needing to wire together abstract classes,
  // C++/Cocoa bridges, and ExtensionMessageBubbleController's somewhat strange
  // Show() interface. It's ugly, but it's pretty confined, so it's probably
  // okay (but if we ever need to expand, it might need to be reconsidered).
  extensions::ExtensionMessageBubbleController* weak_controller =
      bubble_controller.get();
  scoped_ptr<ExtensionMessageBubbleBridge> bridge(
      new ExtensionMessageBubbleBridge(std::move(bubble_controller),
                                       anchor_action != nullptr));
  ToolbarActionsBarBubbleMac* bubble =
      [controller_ createMessageBubble:std::move(bridge)
                          anchorToSelf:anchor_action != nil];
  weak_controller->OnShown();
  [bubble showWindow:nil];
}

}  // namespace

@implementation BrowserActionsController

@synthesize containerView = containerView_;
@synthesize browser = browser_;
@synthesize isOverflow = isOverflow_;
@synthesize activeBubble = activeBubble_;

#pragma mark -
#pragma mark Public Methods

- (id)initWithBrowser:(Browser*)browser
        containerView:(BrowserActionsContainerView*)container
       mainController:(BrowserActionsController*)mainController {
  DCHECK(browser && container);

  if ((self = [super init])) {
    browser_ = browser;
    isOverflow_ = mainController != nil;

    toolbarActionsBarBridge_.reset(new ToolbarActionsBarBridge(self));
    ToolbarActionsBar* mainBar =
        mainController ? [mainController toolbarActionsBar] : nullptr;
    toolbarActionsBar_.reset(
        new ToolbarActionsBar(toolbarActionsBarBridge_.get(),
                              browser_,
                              mainBar));

    containerView_ = container;
    [containerView_ setPostsFrameChangedNotifications:YES];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(containerFrameChanged:)
               name:NSViewFrameDidChangeNotification
             object:containerView_];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(containerDragStart:)
               name:kBrowserActionGrippyDragStartedNotification
             object:containerView_];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(containerDragFinished:)
               name:kBrowserActionGrippyDragFinishedNotification
             object:containerView_];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(containerAnimationEnded:)
               name:kBrowserActionsContainerAnimationEnded
             object:containerView_];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(containerKeyEvent:)
               name:kBrowserActionsContainerReceivedKeyEvent
             object:containerView_];
    // Listen for a finished drag from any button to make sure each open window
    // stays in sync.
    [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(actionButtonDragFinished:)
             name:kBrowserActionButtonDragEndNotification
           object:nil];

    suppressChevron_ = NO;
    if (toolbarActionsBar_->platform_settings().chevron_enabled) {
      chevronAnimation_.reset([[NSViewAnimation alloc] init]);
      [chevronAnimation_ gtm_setDuration:kAnimationDuration
                               eventMask:NSLeftMouseUpMask];
      [chevronAnimation_ setAnimationBlockingMode:NSAnimationNonblocking];
    }

    if (isOverflow_)
      toolbarActionsBar_->SetOverflowRowWidth(NSWidth([containerView_ frame]));

    buttons_.reset([[NSMutableArray alloc] init]);
    toolbarActionsBar_->CreateActions();
    [self showChevronIfNecessaryInFrame:[containerView_ frame]];
    [self updateGrippyCursors];
    [container setIsOverflow:isOverflow_];

    focusedViewIndex_ = -1;
  }

  return self;
}

- (void)dealloc {
  [self browserWillBeDestroyed];
  [super dealloc];
}

- (void)browserWillBeDestroyed {
  [overflowMenu_ setDelegate:nil];
  // Explicitly destroy the ToolbarActionsBar so all buttons get removed with a
  // valid BrowserActionsController, and so we can verify state before
  // destruction.
  if (toolbarActionsBar_.get()) {
    toolbarActionsBar_->DeleteActions();
    toolbarActionsBar_.reset();
  }
  DCHECK_EQ(0u, [buttons_ count]);
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  browser_ = nullptr;
}

- (void)update {
  toolbarActionsBar_->Update();
}

- (NSUInteger)buttonCount {
  return [buttons_ count];
}

- (NSUInteger)visibleButtonCount {
  NSUInteger visibleCount = 0;
  for (BrowserActionButton* button in buttons_.get())
    visibleCount += [button superview] == containerView_;
  return visibleCount;
}

- (gfx::Size)preferredSize {
  return toolbarActionsBar_->GetPreferredSize();
}

- (NSPoint)popupPointForId:(const std::string&)id {
  BrowserActionButton* button = [self buttonForId:id];
  if (!button)
    return NSZeroPoint;

  NSRect bounds;
  NSView* referenceButton = button;
  if ([button superview] != containerView_ || isOverflow_) {
    referenceButton = toolbarActionsBar_->platform_settings().chevron_enabled ?
         chevronMenuButton_.get() : [[self toolbarController] appMenuButton];
    bounds = [referenceButton bounds];
  } else {
    bounds = [button convertRect:[button frameAfterAnimation]
                        fromView:[button superview]];
  }

  return [self popupPointForView:referenceButton withBounds:bounds];
}

- (BOOL)chevronIsHidden {
  if (!chevronMenuButton_.get())
    return YES;

  if (![chevronAnimation_ isAnimating])
    return [chevronMenuButton_ isHidden];

  DCHECK([[chevronAnimation_ viewAnimations] count] > 0);

  // The chevron is animating in or out. Determine which one and have the return
  // value reflect where the animation is headed.
  NSString* effect = [[[chevronAnimation_ viewAnimations] objectAtIndex:0]
      valueForKey:NSViewAnimationEffectKey];
  if (effect == NSViewAnimationFadeInEffect) {
    return NO;
  } else if (effect == NSViewAnimationFadeOutEffect) {
    return YES;
  }

  NOTREACHED();
  return YES;
}

- (content::WebContents*)currentWebContents {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

- (BrowserActionButton*)mainButtonForId:(const std::string&)id {
  BrowserActionsController* mainController = isOverflow_ ?
      [[self toolbarController] browserActionsController] : self;
  return [mainController buttonForId:id];
}

- (ToolbarActionsBar*)toolbarActionsBar {
  return toolbarActionsBar_.get();
}

- (void)setFocusedInOverflow:(BOOL)focused {
  BOOL isFocused = focusedViewIndex_ != -1;
  if (isFocused != focused) {
    int index = focused ?
        [buttons_ count] - toolbarActionsBar_->GetIconCount() : -1;
    [self setFocusedViewIndex:index];
  }
}

- (gfx::Size)sizeForOverflowWidth:(int)maxWidth {
  toolbarActionsBar_->SetOverflowRowWidth(maxWidth);
  return [self preferredSize];
}

#pragma mark -
#pragma mark NSMenuDelegate

- (void)menuNeedsUpdate:(NSMenu*)menu {
  [menu removeAllItems];

  // See menu_button.h for documentation on why this is needed.
  [menu addItemWithTitle:@"" action:nil keyEquivalent:@""];

  NSUInteger iconCount = toolbarActionsBar_->GetIconCount();
  NSRange hiddenButtonRange =
      NSMakeRange(iconCount, [buttons_ count] - iconCount);
  for (BrowserActionButton* button in
           [buttons_ subarrayWithRange:hiddenButtonRange]) {
    NSString* name =
        base::SysUTF16ToNSString([button viewController]->GetActionName());
    NSMenuItem* item =
        [menu addItemWithTitle:name
                        action:@selector(chevronItemSelected:)
                 keyEquivalent:@""];
    [item setRepresentedObject:button];
    [item setImage:[button compositedImage]];
    [item setTarget:self];
    [item setEnabled:[button isEnabled]];
  }
}

#pragma mark -
#pragma mark Private Methods

- (void)addViewForAction:(ToolbarActionViewController*)action
               withIndex:(NSUInteger)index {
  NSRect buttonFrame = NSMakeRect(NSMaxX([containerView_ bounds]),
                                  0,
                                  ToolbarActionsBar::IconWidth(false),
                                  ToolbarActionsBar::IconHeight());
  BrowserActionButton* newButton =
      [[[BrowserActionButton alloc]
         initWithFrame:buttonFrame
        viewController:action
            controller:self] autorelease];
  [newButton setTarget:self];
  [newButton setAction:@selector(browserActionClicked:)];
  [buttons_ insertObject:newButton atIndex:index];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(actionButtonDragging:)
             name:kBrowserActionButtonDraggingNotification
           object:newButton];

  [containerView_ setMaxDesiredWidth:toolbarActionsBar_->GetMaximumWidth()];
}

- (void)redraw {
  if (![self updateContainerVisibility])
    return;  // Container is hidden; no need to update.

  scoped_ptr<ui::NinePartImageIds> highlight;
  if (toolbarActionsBar_->is_highlighting()) {
    if (toolbarActionsBar_->highlight_type() ==
        ToolbarActionsModel::HIGHLIGHT_INFO)
      highlight.reset(
          new ui::NinePartImageIds(IMAGE_GRID(IDR_TOOLBAR_ACTION_HIGHLIGHT)));
    else
      highlight.reset(
          new ui::NinePartImageIds(IMAGE_GRID(IDR_DEVELOPER_MODE_HIGHLIGHT)));
  }
  [containerView_ setHighlight:std::move(highlight)];
  if (toolbarActionsBar_->show_icon_surfacing_bubble() &&
      ![containerView_ trackingEnabled]) {
    [containerView_ setTrackingEnabled:YES];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(containerMouseEntered:)
               name:kBrowserActionsContainerMouseEntered
             object:containerView_];
  }

  std::vector<ToolbarActionViewController*> toolbar_actions =
      toolbarActionsBar_->GetActions();
  for (NSUInteger i = 0; i < [buttons_ count]; ++i) {
    ToolbarActionViewController* controller =
        [[self buttonAtIndex:i] viewController];
    if (controller != toolbar_actions[i]) {
      size_t j = i + 1;
      while (true) {
        ToolbarActionViewController* other_controller =
            [[self buttonAtIndex:j] viewController];
        if (other_controller == toolbar_actions[i])
          break;
        ++j;
      }
      [buttons_ exchangeObjectAtIndex:i withObjectAtIndex:j];
    }
  }

  [self showChevronIfNecessaryInFrame:[containerView_ frame]];
  NSUInteger startIndex = toolbarActionsBar_->GetStartIndexInBounds();
  NSUInteger endIndex = toolbarActionsBar_->GetEndIndexInBounds();
  for (NSUInteger i = 0; i < [buttons_ count]; ++i) {
    BrowserActionButton* button = [buttons_ objectAtIndex:i];
    if ([button isBeingDragged])
      continue;

    [self moveButton:[buttons_ objectAtIndex:i] toIndex:i];

    if (i >= startIndex && i < endIndex) {
      // Make sure the button is within the visible container.
      if ([button superview] != containerView_) {
        // We add the subview under the sibling views so that when it
        // "slides in", it does so under its neighbors.
        [containerView_ addSubview:button
                        positioned:NSWindowBelow
                        relativeTo:nil];
      }
      // We need to set the alpha value in case the container has resized.
      [button setAlphaValue:1.0];
    } else if ([button superview] == containerView_ &&
               ![containerView_ userIsResizing]) {
      // If the user is resizing, all buttons are (and should be) on the
      // container view.
      [button removeFromSuperview];
      [button setAlphaValue:0.0];
    }
  }
}

- (void)removeViewForAction:(ToolbarActionViewController*)action {
  BrowserActionButton* button = [self buttonForId:action->GetId()];

  [button removeFromSuperview];
  [button onRemoved];
  [buttons_ removeObject:button];

  [containerView_ setMaxDesiredWidth:toolbarActionsBar_->GetMaximumWidth()];
}

- (void)removeAllViews {
  for (BrowserActionButton* button in buttons_.get()) {
    [button removeFromSuperview];
    [button onRemoved];
  }
  [buttons_ removeAllObjects];
}

- (void)resizeContainerToWidth:(CGFloat)width {
  // Cocoa goes a little crazy if we try and change animations while adjusting
  // child frames (i.e., the buttons). If the toolbar is already animating,
  // just jump to the new frame. (This typically only happens if someone is
  // "spamming" a button to add/remove an action.)
  BOOL animate = !toolbarActionsBar_->suppress_animation() &&
      ![containerView_ isAnimating];
  [self updateContainerVisibility];
  [containerView_ resizeToWidth:width
                        animate:animate];
  NSRect frame = animate ? [containerView_ animationEndFrame] :
                           [containerView_ frame];

  [self showChevronIfNecessaryInFrame:frame];

  [containerView_ setNeedsDisplay:YES];

  if (!animate) {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kBrowserActionVisibilityChangedNotification
                      object:self];
  }
  [self redraw];
  [self updateGrippyCursors];
}

- (BOOL)updateContainerVisibility {
  BOOL hidden = [buttons_ count] == 0;
  if ([containerView_ isHidden] != hidden)
    [containerView_ setHidden:hidden];
  return !hidden;
}

- (void)updateButtonOpacity {
  for (BrowserActionButton* button in buttons_.get()) {
    NSRect buttonFrame = [button frameAfterAnimation];
    // The button is fully in the container view, and should get full opacity.
    if (NSContainsRect([containerView_ bounds], buttonFrame)) {
      if ([button alphaValue] != 1.0)
        [button setAlphaValue:1.0];

      continue;
    }
    // The button is only partially in the container view. If the user is
    // resizing the container, we have partial alpha so the icon fades in as
    // space is made. Otherwise, hide the icon fully.
    CGFloat alpha = 0.0;
    if ([containerView_ userIsResizing]) {
      CGFloat intersectionWidth =
          NSWidth(NSIntersectionRect([containerView_ bounds], buttonFrame));
      alpha = std::max(static_cast<CGFloat>(0.0),
                       intersectionWidth / NSWidth(buttonFrame));
    }
    [button setAlphaValue:alpha];
    [button setNeedsDisplay:YES];
  }
}

- (void)updateButtonPositions {
  for (NSUInteger index = 0; index < [buttons_ count]; ++index) {
    BrowserActionButton* button = [buttons_ objectAtIndex:index];
    NSRect buttonFrame = [self frameForIndex:index];

    // If the button is at the proper position (or animating to it), then we
    // don't need to update its position.
    if (NSMinX([button frameAfterAnimation]) == NSMinX(buttonFrame))
      continue;

    // We set the x-origin by calculating the proper distance from the right
    // edge in the container so that, if the container is animating, the
    // button appears stationary.
    buttonFrame.origin.x = NSWidth([containerView_ frame]) -
        (toolbarActionsBar_->GetPreferredSize().width() - NSMinX(buttonFrame));
    [button setFrame:buttonFrame animate:NO];
  }
}

- (BrowserActionButton*)buttonForId:(const std::string&)id {
  for (BrowserActionButton* button in buttons_.get()) {
    if ([button viewController]->GetId() == id)
      return button;
  }
  return nil;
}

- (BrowserActionButton*)buttonAtIndex:(NSUInteger)index {
  return static_cast<BrowserActionButton*>([buttons_ objectAtIndex:index]);
}

- (void)containerFrameChanged:(NSNotification*)notification {
  [self updateButtonPositions];
  [self updateButtonOpacity];
  [[containerView_ window] invalidateCursorRectsForView:containerView_];
  [self updateChevronPositionInFrame:[containerView_ frame]];
}

- (void)containerDragStart:(NSNotification*)notification {
  [self setChevronHidden:YES inFrame:[containerView_ frame]];
  for (BrowserActionButton* button in buttons_.get()) {
    if ([button superview] != containerView_) {
      [button setAlphaValue:1.0];
      [containerView_ addSubview:button];
    }
  }
}

- (void)containerDragFinished:(NSNotification*)notification {
  for (BrowserActionButton* button in buttons_.get()) {
    NSRect buttonFrame = [button frame];
    if (NSContainsRect([containerView_ bounds], buttonFrame))
      continue;

    CGFloat intersectionWidth =
        NSWidth(NSIntersectionRect([containerView_ bounds], buttonFrame));
    // Hide the button if it's not "mostly" visible. "Mostly" here equates to
    // having three or fewer pixels hidden.
    if (([containerView_ grippyPinned] && intersectionWidth > 0) ||
        (intersectionWidth <= NSWidth(buttonFrame) - 3.0)) {
      [button setAlphaValue:0.0];
      [button removeFromSuperview];
    }
  }

  toolbarActionsBar_->OnResizeComplete(
      toolbarActionsBar_->IconCountToWidth([self visibleButtonCount]));

  [self updateGrippyCursors];
  [self resizeContainerToWidth:toolbarActionsBar_->GetPreferredSize().width()];
}

- (void)containerAnimationEnded:(NSNotification*)notification {
  if (![containerView_ isAnimating])
    toolbarActionsBar_->OnAnimationEnded();
}

- (void)containerKeyEvent:(NSNotification*)notification {
  DCHECK(isOverflow_);  // We only manually process key events in overflow.

  NSDictionary* dict = [notification userInfo];
  BrowserActionsContainerKeyAction action =
      static_cast<BrowserActionsContainerKeyAction>(
          [[dict objectForKey:kBrowserActionsContainerKeyEventKey] intValue]);
  switch (action) {
    case BROWSER_ACTIONS_DECREMENT_FOCUS:
    case BROWSER_ACTIONS_INCREMENT_FOCUS: {
      NSInteger newIndex = focusedViewIndex_ +
          (action == BROWSER_ACTIONS_INCREMENT_FOCUS ? 1 : -1);
      NSInteger minIndex =
          [buttons_ count] - toolbarActionsBar_->GetIconCount();
      if (newIndex >= minIndex && newIndex < static_cast<int>([buttons_ count]))
        [self setFocusedViewIndex:newIndex];
      break;
    }
    case BROWSER_ACTIONS_EXECUTE_CURRENT: {
      if (focusedViewIndex_ != -1) {
        BrowserActionButton* focusedButton =
            [self buttonAtIndex:focusedViewIndex_];
        [focusedButton performClick:focusedButton];
      }
      break;
    }
    case BROWSER_ACTIONS_INVALID_KEY_ACTION:
      NOTREACHED();
  }
}

- (void)containerMouseEntered:(NSNotification*)notification {
  if (!activeBubble_ &&  // only show one bubble at a time
      toolbarActionsBar_->show_icon_surfacing_bubble()) {
    scoped_ptr<ToolbarActionsBarBubbleDelegate> delegate(
        new ExtensionToolbarIconSurfacingBubbleDelegate(browser_->profile()));
    ToolbarActionsBarBubbleMac* bubble =
        [self createMessageBubble:std::move(delegate) anchorToSelf:YES];
    [bubble showWindow:nil];
  }
  [containerView_ setTrackingEnabled:NO];
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:kBrowserActionsContainerMouseEntered
              object:containerView_];
}

- (void)actionButtonDragging:(NSNotification*)notification {
  suppressChevron_ = YES;
  if (![self chevronIsHidden])
    [self setChevronHidden:YES inFrame:[containerView_ frame]];

  // Determine what index the dragged button should lie in, alter the model and
  // reposition the buttons.
  BrowserActionButton* draggedButton = [notification object];
  NSRect draggedButtonFrame = [draggedButton frame];
  // Find the mid-point. We flip the y-coordinates so that y = 0 is at the
  // top of the container to make row calculation more logical.
  NSPoint midPoint =
      NSMakePoint(NSMidX(draggedButtonFrame),
                  NSMaxY([containerView_ bounds]) - NSMidY(draggedButtonFrame));

  // Calculate the row index and the index in the row. We bound the latter
  // because the view can go farther right than the right-most icon in the last
  // row of the overflow menu.
  NSInteger rowIndex = midPoint.y / ToolbarActionsBar::IconHeight();
  int icons_per_row = isOverflow_ ?
      toolbarActionsBar_->platform_settings().icons_per_overflow_menu_row :
      toolbarActionsBar_->GetIconCount();
  NSInteger indexInRow = std::min(icons_per_row - 1,
      static_cast<int>(midPoint.x / ToolbarActionsBar::IconWidth(true)));

  // Find the desired index for the button.
  NSInteger maxIndex = [buttons_ count] - 1;
  NSInteger offset = isOverflow_ ?
      [buttons_ count] - toolbarActionsBar_->GetIconCount() : 0;
  NSInteger index =
      std::min(maxIndex, offset + rowIndex * icons_per_row + indexInRow);

  toolbarActionsBar_->OnDragDrop([buttons_ indexOfObject:draggedButton],
                                 index,
                                 ToolbarActionsBar::DRAG_TO_SAME);
}

- (void)actionButtonDragFinished:(NSNotification*)notification {
  suppressChevron_ = NO;
  [self redraw];
}

- (NSRect)frameForIndex:(NSUInteger)index {
  gfx::Rect frameRect = toolbarActionsBar_->GetFrameForIndex(index);
  int iconWidth = ToolbarActionsBar::IconWidth(false);
  // The toolbar actions bar will return an empty rect if the index is for an
  // action that is before range we show (i.e., is for a button that's on the
  // main bar, and this is the overflow). Set the frame to be outside the bounds
  // of the view.
  NSRect frame = frameRect.IsEmpty() ?
      NSMakeRect(-iconWidth - 1, 0, iconWidth,
                 ToolbarActionsBar::IconHeight()) :
      NSRectFromCGRect(frameRect.ToCGRect());
  // We need to flip the y coordinate for Cocoa's view system.
  frame.origin.y = NSHeight([containerView_ frame]) - NSMaxY(frame);
  return frame;
}

- (NSPoint)popupPointForView:(NSView*)view
                  withBounds:(NSRect)bounds {
  // Anchor point just above the center of the bottom.
  int y = [view isFlipped] ? NSMaxY(bounds) - kBrowserActionBubbleYOffset :
                             kBrowserActionBubbleYOffset;
  NSPoint anchor = NSMakePoint(NSMidX(bounds), y);
  // Convert the point to the container view's frame, and adjust for animation.
  NSPoint anchorInContainer =
      [containerView_ convertPoint:anchor fromView:view];
  anchorInContainer.x -= NSMinX([containerView_ frame]) -
      NSMinX([containerView_ animationEndFrame]);

  return [containerView_ convertPoint:anchorInContainer toView:nil];
}

- (void)moveButton:(BrowserActionButton*)button
           toIndex:(NSUInteger)index {
  NSRect buttonFrame = [self frameForIndex:index];

  CGFloat currentX = NSMinX([button frame]);
  CGFloat xLeft = toolbarActionsBar_->GetPreferredSize().width() -
      NSMinX(buttonFrame);
  // We check if the button is already in the correct place for the toolbar's
  // current size. This could mean that the button could be the correct distance
  // from the left or from the right edge. If it has the correct distance, we
  // don't move it, and it will be updated when the container frame changes.
  // This way, if the user has extensions A and C installed, and installs
  // extension B between them, extension C appears to stay stationary on the
  // screen while the toolbar expands to the left (even though C's bounds within
  // the container change).
  if ((currentX == NSMinX(buttonFrame) ||
       currentX == NSWidth([containerView_ frame]) - xLeft) &&
      NSMinY([button frame]) == NSMinY(buttonFrame)) {
    // If the button is in the right place, but animating, we need to stop the
    // animation.
    if ([button isAnimating])
      [button stopAnimation];
    return;
  }

  // It's possible the button is already animating to the right place. Don't
  // call move again, because it will stop the current animation.
  if (!NSEqualRects(buttonFrame, [button frameAfterAnimation])) {
    [button setFrame:buttonFrame
             animate:!toolbarActionsBar_->suppress_animation() && !isOverflow_];
  }
}

- (BOOL)browserActionClicked:(BrowserActionButton*)button {
  return [button viewController]->ExecuteAction(true);
}

- (void)showChevronIfNecessaryInFrame:(NSRect)frame {
  if (!toolbarActionsBar_->platform_settings().chevron_enabled)
    return;
  bool hidden = suppressChevron_ ||
      toolbarActionsBar_->GetIconCount() == [self buttonCount];
  [self setChevronHidden:hidden inFrame:frame];
}

- (void)updateChevronPositionInFrame:(NSRect)frame {
  CGFloat xPos = NSWidth(frame) - kChevronWidth -
      toolbarActionsBar_->platform_settings().item_spacing;
  NSRect buttonFrame = NSMakeRect(xPos,
                                  0,
                                  kChevronWidth,
                                  ToolbarActionsBar::IconHeight());
  [chevronAnimation_ stopAnimation];
  [chevronMenuButton_ setFrame:buttonFrame];
}

- (void)setChevronHidden:(BOOL)hidden
                 inFrame:(NSRect)frame {
  if (!toolbarActionsBar_->platform_settings().chevron_enabled ||
      hidden == [self chevronIsHidden])
    return;

  if (!chevronMenuButton_.get()) {
    chevronMenuButton_.reset([[MenuButton alloc] init]);
    [chevronMenuButton_ setOpenMenuOnClick:YES];
    [chevronMenuButton_ setBordered:NO];
    [chevronMenuButton_ setShowsBorderOnlyWhileMouseInside:YES];

    [[chevronMenuButton_ cell] setImageID:IDR_BROWSER_ACTIONS_OVERFLOW
                           forButtonState:image_button_cell::kDefaultState];
    [[chevronMenuButton_ cell] setImageID:IDR_BROWSER_ACTIONS_OVERFLOW_H
                           forButtonState:image_button_cell::kHoverState];
    [[chevronMenuButton_ cell] setImageID:IDR_BROWSER_ACTIONS_OVERFLOW_P
                           forButtonState:image_button_cell::kPressedState];

    overflowMenu_.reset([[NSMenu alloc] initWithTitle:@""]);
    [overflowMenu_ setAutoenablesItems:NO];
    [overflowMenu_ setDelegate:self];
    [chevronMenuButton_ setAttachedMenu:overflowMenu_];

    [containerView_ addSubview:chevronMenuButton_];
  }

  [self updateChevronPositionInFrame:frame];

  // Stop any running animation.
  [chevronAnimation_ stopAnimation];

  if (toolbarActionsBar_->suppress_animation()) {
    [chevronMenuButton_ setHidden:hidden];
    return;
  }

  NSString* animationEffect;
  if (hidden) {
    animationEffect = NSViewAnimationFadeOutEffect;
  } else {
    [chevronMenuButton_ setHidden:NO];
    animationEffect = NSViewAnimationFadeInEffect;
  }
  NSDictionary* animationDictionary = @{
      NSViewAnimationTargetKey : chevronMenuButton_.get(),
      NSViewAnimationEffectKey : animationEffect
  };
  [chevronAnimation_ setViewAnimations:
      [NSArray arrayWithObject:animationDictionary]];
  [chevronAnimation_ startAnimation];
}

- (void)chevronItemSelected:(id)menuItem {
  [self browserActionClicked:[menuItem representedObject]];
}

- (void)updateGrippyCursors {
  [containerView_
      setCanDragLeft:toolbarActionsBar_->GetIconCount() != [buttons_ count]];
  [containerView_ setCanDragRight:[self visibleButtonCount] > 0];
  [[containerView_ window] invalidateCursorRectsForView:containerView_];
}

- (ToolbarController*)toolbarController {
  return [[BrowserWindowController browserWindowControllerForWindow:
             browser_->window()->GetNativeWindow()] toolbarController];
}

- (ToolbarActionsBarBubbleMac*)createMessageBubble:
    (scoped_ptr<ToolbarActionsBarBubbleDelegate>)delegate
    anchorToSelf:(BOOL)anchorToSelf {
  DCHECK_GE([buttons_ count], 0u);
  NSView* anchorView =
      anchorToSelf ? containerView_ : [[self toolbarController] appMenuButton];
  NSPoint anchor = [self popupPointForView:anchorView
                                withBounds:[anchorView bounds]];

  anchor = [[containerView_ window] convertBaseToScreen:anchor];
  activeBubble_ = [[ToolbarActionsBarBubbleMac alloc]
      initWithParentWindow:[containerView_ window]
               anchorPoint:anchor
                  delegate:std::move(delegate)];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(bubbleWindowClosing:)
             name:NSWindowWillCloseNotification
           object:[activeBubble_ window]];
  return activeBubble_;
}

- (void)bubbleWindowClosing:(NSNotification*)notification {
  activeBubble_ = nil;
}

- (void)setFocusedViewIndex:(NSInteger)index {
  DCHECK(isOverflow_);
  focusedViewIndex_ = index;
}

#pragma mark -
#pragma mark Testing Methods

- (BrowserActionButton*)buttonWithIndex:(NSUInteger)index {
  return index < [buttons_ count] ? [buttons_ objectAtIndex:index] : nil;
}

@end
