// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/website_settings/chooser_bubble_ui_cocoa.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_utils.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_button.h"
#import "chrome/browser/ui/cocoa/hover_close_button.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/website_settings/chooser_bubble_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

// Chooser bubble width.
const CGFloat kChooserBubbleWidth = 320.0f;

// Chooser bubble height.
const CGFloat kChooserBubbleHeight = 220.0f;

// Distance between the bubble border and the view that is closest to the
// border.
const CGFloat kMarginX = 20.0f;
const CGFloat kMarginY = 20.0f;

// Distance between two views inside the bubble.
const CGFloat kHorizontalPadding = 10.0f;
const CGFloat kVerticalPadding = 10.0f;

const CGFloat kTitlePaddingX = 50.0f;
}

scoped_ptr<BubbleUi> ChooserBubbleDelegate::BuildBubbleUi() {
  return make_scoped_ptr(new ChooserBubbleUiCocoa(browser_, this));
}

@interface ChooserBubbleUiController
    : BaseBubbleController<NSTableViewDataSource, NSTableViewDelegate> {
 @private
  // Bridge to the C++ class that created this object.
  ChooserBubbleUiCocoa* bridge_;

  base::scoped_nsobject<NSTextField> titleView_;
  base::scoped_nsobject<NSScrollView> scrollView_;
  base::scoped_nsobject<NSTableColumn> tableColumn_;
  base::scoped_nsobject<NSTableView> tableView_;
  base::scoped_nsobject<NSButton> connectButton_;
  base::scoped_nsobject<NSButton> cancelButton_;

  Browser* browser_;                                // Weak.
  ChooserBubbleDelegate* chooserBubbleDelegate_;    // Weak.
}

// Designated initializer.  |browser| and |bridge| must both be non-nil.
- (id)initWithBrowser:(Browser*)browser
    initWithChooserBubbleDelegate:(ChooserBubbleDelegate*)chooserBubbleDelegate
                           bridge:(ChooserBubbleUiCocoa*)bridge;

// Makes the bubble visible.
- (void)show;

// Will reposition the bubble based in case the anchor or parent should change.
- (void)updateAnchorPosition;

// Will calculate the expected anchor point for this bubble.
// Should only be used outside this class for tests.
- (NSPoint)getExpectedAnchorPoint;

// Returns true if the browser has support for the location bar.
// Should only be used outside this class for tests.
- (bool)hasLocationBar;

// Update |tableView_| when chooser options were initialized.
- (void)onOptionsInitialized;

// Update |tableView_| when chooser option was added.
- (void)onOptionAdded:(NSInteger)index;

// Update |tableView_| when chooser option was removed.
- (void)onOptionRemoved:(NSInteger)index;

// Update |tableView_| when chooser options changed.
- (void)updateTableView;

// Determines if the bubble has an anchor in a corner or no anchor at all.
- (info_bubble::BubbleArrowLocation)getExpectedArrowLocation;

// Returns the expected parent for this bubble.
- (NSWindow*)getExpectedParentWindow;

// Creates the title for the bubble.
- (base::scoped_nsobject<NSTextField>)bubbleTitle;

// Creates a button with |title| and |action|.
- (base::scoped_nsobject<NSButton>)buttonWithTitle:(NSString*)title
                                            action:(SEL)action;

// Creates the "Connect" button.
- (base::scoped_nsobject<NSButton>)connectButton;

// Creates the "Cancel" button.
- (base::scoped_nsobject<NSButton>)cancelButton;

// Called when the "Connect" button is pressed.
- (void)onConnect:(id)sender;

// Called when the "Cancel" button is pressed.
- (void)onCancel:(id)sender;

@end

@implementation ChooserBubbleUiController

- (id)initWithBrowser:(Browser*)browser
    initWithChooserBubbleDelegate:(ChooserBubbleDelegate*)chooserBubbleDelegate
                           bridge:(ChooserBubbleUiCocoa*)bridge {
  DCHECK(browser);
  DCHECK(chooserBubbleDelegate);
  DCHECK(bridge);

  browser_ = browser;
  chooserBubbleDelegate_ = chooserBubbleDelegate;

  base::scoped_nsobject<InfoBubbleWindow> window([[InfoBubbleWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSBorderlessWindowMask
                  backing:NSBackingStoreBuffered
                    defer:NO]);
  [window setAllowedAnimations:info_bubble::kAnimateNone];
  [window setReleasedWhenClosed:NO];
  if ((self = [super initWithWindow:window
                       parentWindow:[self getExpectedParentWindow]
                         anchoredAt:NSZeroPoint])) {
    [self setShouldCloseOnResignKey:NO];
    [self setShouldOpenAsKeyWindow:YES];
    [[self bubble] setArrowLocation:[self getExpectedArrowLocation]];
    bridge_ = bridge;
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(parentWindowDidMove:)
                   name:NSWindowDidMoveNotification
                 object:[self getExpectedParentWindow]];
  }
  return self;
}

- (void)windowWillClose:(NSNotification*)notification {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:NSWindowDidMoveNotification
              object:nil];
  bridge_->OnBubbleClosing();
  [super windowWillClose:notification];
}

- (void)parentWindowWillToggleFullScreen:(NSNotification*)notification {
  // Override the base class implementation, which would have closed the bubble.
}

- (void)parentWindowDidResize:(NSNotification*)notification {
  [self setAnchorPoint:[self getExpectedAnchorPoint]];
}

- (void)parentWindowDidMove:(NSNotification*)notification {
  DCHECK(bridge_);
  [self setAnchorPoint:[self getExpectedAnchorPoint]];
}

- (void)show {
  NSView* view = [[self window] contentView];

  // ------------------------------------
  // | Chooser bubble title             |
  // | -------------------------------- |
  // | | option 0                     | |
  // | | option 1                     | |
  // | | option 2                     | |
  // | |                              | |
  // | |                              | |
  // | |                              | |
  // | -------------------------------- |
  // |            [ Connect] [ Cancel ] |
  // ------------------------------------

  // Determine the dimensions of the bubble.
  // Once the height and width are set, the buttons and permission menus can
  // be laid out correctly.
  NSRect bubbleFrame =
      NSMakeRect(0, 0, kChooserBubbleWidth, kChooserBubbleHeight);

  // Create the views.
  // Title.
  titleView_ = [self bubbleTitle];
  CGFloat titleOriginX = kMarginX;
  CGFloat titleHeight = NSHeight([titleView_ frame]);
  CGFloat titleOriginY = kChooserBubbleHeight - kMarginY - titleHeight;
  [titleView_ setFrameOrigin:NSMakePoint(titleOriginX, titleOriginY)];
  [view addSubview:titleView_];

  // Connect button.
  connectButton_ = [self connectButton];
  // Cancel button.
  cancelButton_ = [self cancelButton];
  CGFloat connectButtonWidth = NSWidth([connectButton_ frame]);
  CGFloat connectButtonHeight = NSHeight([connectButton_ frame]);
  CGFloat cancelButtonWidth = NSWidth([cancelButton_ frame]);

  // ScollView embedding with TableView.
  CGFloat scrollViewWidth = kChooserBubbleWidth - 2 * kMarginX;
  CGFloat scrollViewHeight = kChooserBubbleHeight - 2 * kMarginY -
                             2 * kVerticalPadding - titleHeight -
                             connectButtonHeight;
  NSRect scrollFrame =
      NSMakeRect(kMarginX, kMarginY + connectButtonHeight + kVerticalPadding,
                 scrollViewWidth, scrollViewHeight);
  scrollView_.reset([[NSScrollView alloc] initWithFrame:scrollFrame]);
  [scrollView_ setBorderType:NSBezelBorder];
  [scrollView_ setHasVerticalScroller:YES];
  [scrollView_ setHasHorizontalScroller:YES];
  [scrollView_ setAutohidesScrollers:YES];

  // TableView.
  tableView_.reset([[NSTableView alloc] initWithFrame:NSZeroRect]);
  tableColumn_.reset([[NSTableColumn alloc] initWithIdentifier:@""]);
  [tableColumn_ setWidth:(scrollViewWidth - kMarginX)];
  [tableView_ addTableColumn:tableColumn_];
  [tableView_ setDelegate:self];
  [tableView_ setDataSource:self];
  // Make the column title invisible.
  [tableView_ setHeaderView:nil];
  [tableView_ setFocusRingType:NSFocusRingTypeNone];

  [scrollView_ setDocumentView:tableView_];
  [view addSubview:scrollView_];

  // Set connect button and cancel button to the right place.
  CGFloat connectButtonOriginX = kChooserBubbleWidth - kMarginX -
                                 kHorizontalPadding - connectButtonWidth -
                                 cancelButtonWidth;
  CGFloat connectButtonOriginY = kMarginY;
  [connectButton_
      setFrameOrigin:NSMakePoint(connectButtonOriginX, connectButtonOriginY)];
  [connectButton_ setEnabled:NO];
  [view addSubview:connectButton_];

  CGFloat cancelButtonOriginX =
      kChooserBubbleWidth - kMarginX - cancelButtonWidth;
  CGFloat cancelButtonOriginY = kMarginY;
  [cancelButton_
      setFrameOrigin:NSMakePoint(cancelButtonOriginX, cancelButtonOriginY)];
  [view addSubview:cancelButton_];

  bubbleFrame = [[self window] frameRectForContentRect:bubbleFrame];
  if ([[self window] isVisible]) {
    // Unfortunately, calling -setFrame followed by -setFrameOrigin  (called
    // within -setAnchorPoint) causes flickering.  Avoid the flickering by
    // manually adjusting the new frame's origin so that the top left stays the
    // same, and only calling -setFrame.
    NSRect currentWindowFrame = [[self window] frame];
    bubbleFrame.origin = currentWindowFrame.origin;
    bubbleFrame.origin.y = bubbleFrame.origin.y +
                           currentWindowFrame.size.height -
                           bubbleFrame.size.height;
    [[self window] setFrame:bubbleFrame display:YES];
  } else {
    [[self window] setFrame:bubbleFrame display:NO];
    [self setAnchorPoint:[self getExpectedAnchorPoint]];
    [self showWindow:nil];
    [[self window] makeFirstResponder:nil];
    [[self window] setInitialFirstResponder:connectButton_.get()];
  }
}

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView {
  const std::vector<base::string16>& device_names =
      chooserBubbleDelegate_->GetOptions();
  if (device_names.empty()) {
    return 1;
  } else {
    return static_cast<NSInteger>(device_names.size());
  }
}

- (id)tableView:(NSTableView*)tableView
    objectValueForTableColumn:(NSTableColumn*)tableColumn
                          row:(NSInteger)rowIndex {
  const std::vector<base::string16>& device_names =
      chooserBubbleDelegate_->GetOptions();
  if (device_names.empty()) {
    DCHECK(rowIndex == 0);
    return l10n_util::GetNSString(IDS_CHOOSER_BUBBLE_NO_DEVICES_FOUND_PROMPT);
  } else {
    if (rowIndex >= 0 &&
        rowIndex < static_cast<NSInteger>(device_names.size())) {
      return base::SysUTF16ToNSString(device_names[rowIndex]);
    } else {
      return @"";
    }
  }
}

- (BOOL)tableView:(NSTableView*)aTableView
    shouldEditTableColumn:(NSTableColumn*)aTableColumn
                      row:(NSInteger)rowIndex {
  return NO;
}

- (void)onOptionsInitialized {
  [self updateTableView];
}

- (void)onOptionAdded:(NSInteger)index {
  [self updateTableView];
}

- (void)onOptionRemoved:(NSInteger)index {
  // |tableView_| will automatically selects the next item if the current
  // item is removed, so here it tracks if the removed item is the item
  // that was previously selected, if so, deselect it.
  if ([tableView_ selectedRow] == index)
    [tableView_ deselectRow:index];

  [self updateTableView];
}

- (void)updateTableView {
  const std::vector<base::string16>& device_names =
      chooserBubbleDelegate_->GetOptions();
  [tableView_ setEnabled:!device_names.empty()];
  [tableView_ reloadData];
}

- (void)tableViewSelectionDidChange:(NSNotification*)aNotification {
  [connectButton_ setEnabled:[tableView_ numberOfSelectedRows] > 0];
}

- (void)updateAnchorPosition {
  [self setParentWindow:[self getExpectedParentWindow]];
  [self setAnchorPoint:[self getExpectedAnchorPoint]];
}

- (NSPoint)getExpectedAnchorPoint {
  NSPoint anchor;
  if ([self hasLocationBar]) {
    LocationBarViewMac* locationBar =
        [[[self getExpectedParentWindow] windowController] locationBarBridge];
    anchor = locationBar->GetPageInfoBubblePoint();
  } else {
    // Center the bubble if there's no location bar.
    NSRect contentFrame = [[[self getExpectedParentWindow] contentView] frame];
    anchor = NSMakePoint(NSMidX(contentFrame), NSMaxY(contentFrame));
  }

  return [[self getExpectedParentWindow] convertBaseToScreen:anchor];
}

- (bool)hasLocationBar {
  return browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR);
}

- (info_bubble::BubbleArrowLocation)getExpectedArrowLocation {
  return [self hasLocationBar] ? info_bubble::kTopLeft : info_bubble::kNoArrow;
}

- (NSWindow*)getExpectedParentWindow {
  DCHECK(browser_->window());
  return browser_->window()->GetNativeWindow();
}

- (base::scoped_nsobject<NSTextField>)bubbleTitle {
  base::scoped_nsobject<NSTextField> titleView(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  [titleView setDrawsBackground:NO];
  [titleView setBezeled:NO];
  [titleView setEditable:NO];
  [titleView setSelectable:NO];
  [titleView setStringValue:l10n_util::GetNSString(IDS_CHOOSER_BUBBLE_PROMPT)];
  [titleView setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];
  [titleView sizeToFit];
  NSRect titleFrame = [titleView frame];
  [titleView setFrameSize:NSMakeSize(NSWidth(titleFrame) + kTitlePaddingX,
                                     NSHeight(titleFrame))];
  return titleView;
}

- (base::scoped_nsobject<NSButton>)buttonWithTitle:(NSString*)title
                                            action:(SEL)action {
  base::scoped_nsobject<NSButton> button(
      [[ConstrainedWindowButton alloc] initWithFrame:NSZeroRect]);
  [button setButtonType:NSMomentaryPushInButton];
  [button setTitle:title];
  [button setTarget:self];
  [button setAction:action];
  [button sizeToFit];
  return button;
}

- (base::scoped_nsobject<NSButton>)connectButton {
  NSString* connectTitle =
      l10n_util::GetNSString(IDS_CHOOSER_BUBBLE_CONNECT_BUTTON_TEXT);
  return [self buttonWithTitle:connectTitle action:@selector(onConnect:)];
}

- (base::scoped_nsobject<NSButton>)cancelButton {
  NSString* cancelTitle =
      l10n_util::GetNSString(IDS_CHOOSER_BUBBLE_CANCEL_BUTTON_TEXT);
  return [self buttonWithTitle:cancelTitle action:@selector(onCancel:)];
}

+ (CGFloat)matchWidthsOf:(NSView*)viewA andOf:(NSView*)viewB {
  NSRect frameA = [viewA frame];
  NSRect frameB = [viewB frame];
  CGFloat width = std::max(NSWidth(frameA), NSWidth(frameB));
  [viewA setFrameSize:NSMakeSize(width, NSHeight(frameA))];
  [viewB setFrameSize:NSMakeSize(width, NSHeight(frameB))];
  return width;
}

+ (void)alignCenterOf:(NSView*)viewA verticallyToCenterOf:(NSView*)viewB {
  NSRect frameA = [viewA frame];
  NSRect frameB = [viewB frame];
  frameA.origin.y =
      NSMinY(frameB) + std::floor((NSHeight(frameB) - NSHeight(frameA)) / 2);
  [viewA setFrameOrigin:frameA.origin];
}

- (void)onConnect:(id)sender {
  NSInteger row = [tableView_ selectedRow];
  chooserBubbleDelegate_->Select(row);
  [self close];
}

- (void)onCancel:(id)sender {
  chooserBubbleDelegate_->Cancel();
  [self close];
}

@end

ChooserBubbleUiCocoa::ChooserBubbleUiCocoa(
    Browser* browser,
    ChooserBubbleDelegate* chooser_bubble_delegate)
    : browser_(browser),
      chooser_bubble_delegate_(chooser_bubble_delegate),
      chooser_bubble_ui_controller_(nil) {
  DCHECK(browser);
  DCHECK(chooser_bubble_delegate);
  chooser_bubble_delegate_->set_observer(this);
}

ChooserBubbleUiCocoa::~ChooserBubbleUiCocoa() {
  chooser_bubble_delegate_->set_observer(nullptr);
  [chooser_bubble_ui_controller_ close];
  chooser_bubble_ui_controller_ = nil;
}

void ChooserBubbleUiCocoa::Show(BubbleReference bubble_reference) {
  if (!chooser_bubble_ui_controller_) {
    chooser_bubble_ui_controller_ = [[ChooserBubbleUiController alloc]
                      initWithBrowser:browser_
        initWithChooserBubbleDelegate:chooser_bubble_delegate_
                               bridge:this];
  }

  [chooser_bubble_ui_controller_ show];
  [chooser_bubble_ui_controller_ updateTableView];
}

void ChooserBubbleUiCocoa::Close() {
  [chooser_bubble_ui_controller_ close];
  chooser_bubble_ui_controller_ = nil;
}

void ChooserBubbleUiCocoa::UpdateAnchorPosition() {
  [chooser_bubble_ui_controller_ updateAnchorPosition];
}

void ChooserBubbleUiCocoa::OnOptionsInitialized() {
  [chooser_bubble_ui_controller_ onOptionsInitialized];
}

void ChooserBubbleUiCocoa::OnOptionAdded(int index) {
  [chooser_bubble_ui_controller_ onOptionAdded:index];
}

void ChooserBubbleUiCocoa::OnOptionRemoved(int index) {
  [chooser_bubble_ui_controller_ onOptionRemoved:index];
}

void ChooserBubbleUiCocoa::OnBubbleClosing() {
  chooser_bubble_ui_controller_ = nil;
}
