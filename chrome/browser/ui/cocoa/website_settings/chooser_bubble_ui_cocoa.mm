// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/website_settings/chooser_bubble_ui_cocoa.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
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
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/native_web_keyboard_event.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#import "ui/base/cocoa/controls/hyperlink_button_cell.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// Chooser bubble width.
const CGFloat kChooserBubbleWidth = 320.0f;

// Chooser bubble height.
const CGFloat kChooserBubbleHeight = 280.0f;

// Distance between the bubble border and the view that is closest to the
// border.
const CGFloat kMarginX = 20.0f;
const CGFloat kMarginY = 20.0f;

// Distance between two views inside the bubble.
const CGFloat kHorizontalPadding = 10.0f;
const CGFloat kVerticalPadding = 10.0f;

}  // namespace

std::unique_ptr<BubbleUi> ChooserBubbleController::BuildBubbleUi() {
  return base::WrapUnique(new ChooserBubbleUiCocoa(browser_, this));
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
  base::scoped_nsobject<NSTextField> message_;
  base::scoped_nsobject<NSButton> getHelpButton_;
  bool buttonPressed_;

  Browser* browser_;                                  // Weak.
  ChooserBubbleController* chooserBubbleController_;  // Weak.
}

// Designated initializer.  |browser| and |bridge| must both be non-nil.
- (id)initWithBrowser:(Browser*)browser
    initWithChooserBubbleController:
        (ChooserBubbleController*)chooserBubbleController
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

// Creates the message.
- (base::scoped_nsobject<NSTextField>)message;

// Creates the "Get help" button.
- (base::scoped_nsobject<NSButton>)getHelpButton;

// Called when the "Connect" button is pressed.
- (void)onConnect:(id)sender;

// Called when the "Cancel" button is pressed.
- (void)onCancel:(id)sender;

// Called when the "Get help" button is pressed.
- (void)onGetHelpPressed:(id)sender;

@end

@implementation ChooserBubbleUiController

- (id)initWithBrowser:(Browser*)browser
    initWithChooserBubbleController:
        (ChooserBubbleController*)chooserBubbleController
                             bridge:(ChooserBubbleUiCocoa*)bridge {
  DCHECK(browser);
  DCHECK(chooserBubbleController);
  DCHECK(bridge);

  browser_ = browser;
  chooserBubbleController_ = chooserBubbleController;

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
  if (!buttonPressed_)
    chooserBubbleController_->Close();
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
  // |           [ Connect ] [ Cancel ] |
  // |----------------------------------|
  // | Not seeing your device? Get help |
  // ------------------------------------

  // Determine the dimensions of the bubble.
  // Once the height and width are set, the buttons and permission menus can
  // be laid out correctly.
  NSRect bubbleFrame =
      NSMakeRect(0, 0, kChooserBubbleWidth, kChooserBubbleHeight);

  // Create the views.
  // Title.
  titleView_ = [self bubbleTitle];
  CGFloat titleHeight = NSHeight([titleView_ frame]);

  // Connect button.
  connectButton_ = [self connectButton];
  CGFloat connectButtonWidth = NSWidth([connectButton_ frame]);
  CGFloat connectButtonHeight = NSHeight([connectButton_ frame]);

  // Cancel button.
  cancelButton_ = [self cancelButton];
  CGFloat cancelButtonWidth = NSWidth([cancelButton_ frame]);

  // Message.
  message_ = [self message];
  CGFloat messageWidth = NSWidth([message_ frame]);
  CGFloat messageHeight = NSHeight([message_ frame]);

  // Get help button.
  getHelpButton_ = [self getHelpButton];

  // Separator.
  CGFloat separatorOriginX = 0.0f;
  CGFloat separatorOriginY = kMarginY + messageHeight + kVerticalPadding;
  NSBox* separator =
      [self horizontalSeparatorWithFrame:NSMakeRect(separatorOriginX,
                                                    separatorOriginY,
                                                    kChooserBubbleWidth, 0.0f)];

  // ScollView embedding with TableView.
  CGFloat scrollViewWidth = kChooserBubbleWidth - 2 * kMarginX;
  CGFloat scrollViewHeight = kChooserBubbleHeight - 2 * kMarginY -
                             4 * kVerticalPadding - titleHeight -
                             connectButtonHeight - messageHeight;
  NSRect scrollFrame = NSMakeRect(
      kMarginX,
      kMarginY + messageHeight + 3 * kVerticalPadding + connectButtonHeight,
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

  // Lay out the views.
  // Title.
  CGFloat titleOriginX = kMarginX;
  CGFloat titleOriginY = kChooserBubbleHeight - kMarginY - titleHeight;
  [titleView_ setFrameOrigin:NSMakePoint(titleOriginX, titleOriginY)];
  [view addSubview:titleView_];

  // ScollView.
  [scrollView_ setDocumentView:tableView_];
  [view addSubview:scrollView_];

  // Connect button.
  CGFloat connectButtonOriginX = kChooserBubbleWidth - kMarginX -
                                 kHorizontalPadding - connectButtonWidth -
                                 cancelButtonWidth;
  CGFloat connectButtonOriginY =
      kMarginY + messageHeight + 2 * kVerticalPadding;
  [connectButton_
      setFrameOrigin:NSMakePoint(connectButtonOriginX, connectButtonOriginY)];
  [connectButton_ setEnabled:NO];
  [view addSubview:connectButton_];

  // Cancel button.
  CGFloat cancelButtonOriginX =
      kChooserBubbleWidth - kMarginX - cancelButtonWidth;
  CGFloat cancelButtonOriginY = connectButtonOriginY;
  [cancelButton_
      setFrameOrigin:NSMakePoint(cancelButtonOriginX, cancelButtonOriginY)];
  [view addSubview:cancelButton_];

  // Separator.
  [view addSubview:separator];

  // Message.
  CGFloat messageOriginX = kMarginX;
  CGFloat messageOriginY = kMarginY;
  [message_ setFrameOrigin:NSMakePoint(messageOriginX, messageOriginY)];
  [view addSubview:message_];

  // Get help button.
  getHelpButton_ = [self getHelpButton];
  CGFloat getHelpButtonOriginX =
      kMarginX + messageWidth - kHorizontalPadding / 2;
  CGFloat getHelpButtonOriginY = kMarginY;
  [getHelpButton_
      setFrameOrigin:NSMakePoint(getHelpButtonOriginX, getHelpButtonOriginY)];
  [view addSubview:getHelpButton_];

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
    [[self window] setInitialFirstResponder:tableView_.get()];
  }
}

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView {
  // When there are no devices, the table contains a message saying there are
  // no devices, so the number of rows is always at least 1.
  return std::max(
      static_cast<NSInteger>(chooserBubbleController_->NumOptions()),
      static_cast<NSInteger>(1));
}

- (id)tableView:(NSTableView*)tableView
    objectValueForTableColumn:(NSTableColumn*)tableColumn
                          row:(NSInteger)rowIndex {
  NSInteger num_options =
      static_cast<NSInteger>(chooserBubbleController_->NumOptions());
  if (num_options == 0) {
    DCHECK_EQ(0, rowIndex);
    return l10n_util::GetNSString(IDS_CHOOSER_BUBBLE_NO_DEVICES_FOUND_PROMPT);
  }

  DCHECK_GE(rowIndex, 0);
  DCHECK_LT(rowIndex, num_options);
  return base::SysUTF16ToNSString(
      chooserBubbleController_->GetOption(static_cast<size_t>(rowIndex)));
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
  [tableView_ setEnabled:chooserBubbleController_->NumOptions() > 0];
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

  return ui::ConvertPointFromWindowToScreen([self getExpectedParentWindow],
                                            anchor);
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
  [titleView setStringValue:l10n_util::GetNSStringF(
                                IDS_CHOOSER_BUBBLE_PROMPT,
                                base::ASCIIToUTF16(
                                    chooserBubbleController_->GetOrigin()
                                        .Serialize()))];
  [titleView setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];
  // The height is arbitrary as it will be adjusted later.
  [titleView setFrameSize:NSMakeSize(kChooserBubbleWidth - 2 * kMarginX, 0.0f)];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:titleView];
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

- (base::scoped_nsobject<NSTextField>)message {
  base::scoped_nsobject<NSTextField> messageView(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  [messageView setDrawsBackground:NO];
  [messageView setBezeled:NO];
  [messageView setEditable:NO];
  [messageView setSelectable:NO];
  [messageView
      setStringValue:l10n_util::GetNSStringF(IDS_CHOOSER_BUBBLE_FOOTNOTE_TEXT,
                                             base::string16())];
  [messageView setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];
  [messageView sizeToFit];
  return messageView;
}

- (base::scoped_nsobject<NSButton>)getHelpButton {
  base::scoped_nsobject<NSButton> button(
      [[NSButton alloc] initWithFrame:NSZeroRect]);
  base::scoped_nsobject<HyperlinkButtonCell> cell([[HyperlinkButtonCell alloc]
      initTextCell:l10n_util::GetNSString(
                       IDS_CHOOSER_BUBBLE_GET_HELP_LINK_TEXT)]);
  [button setCell:cell.get()];
  [button sizeToFit];
  [button setTarget:self];
  [button setAction:@selector(onGetHelpPressed:)];
  return button;
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
  buttonPressed_ = true;
  NSInteger row = [tableView_ selectedRow];
  chooserBubbleController_->Select(row);
  [self close];
}

- (void)onCancel:(id)sender {
  buttonPressed_ = true;
  chooserBubbleController_->Cancel();
  [self close];
}

- (void)onGetHelpPressed:(id)sender {
  chooserBubbleController_->OpenHelpCenterUrl();
}

@end

ChooserBubbleUiCocoa::ChooserBubbleUiCocoa(
    Browser* browser,
    ChooserBubbleController* chooser_bubble_controller)
    : browser_(browser),
      chooser_bubble_controller_(chooser_bubble_controller),
      chooser_bubble_ui_controller_(nil) {
  DCHECK(browser);
  DCHECK(chooser_bubble_controller);
  chooser_bubble_controller_->set_observer(this);
}

ChooserBubbleUiCocoa::~ChooserBubbleUiCocoa() {
  chooser_bubble_controller_->set_observer(nullptr);
  [chooser_bubble_ui_controller_ close];
  chooser_bubble_ui_controller_ = nil;
}

void ChooserBubbleUiCocoa::Show(BubbleReference bubble_reference) {
  if (!chooser_bubble_ui_controller_) {
    chooser_bubble_ui_controller_ = [[ChooserBubbleUiController alloc]
                        initWithBrowser:browser_
        initWithChooserBubbleController:chooser_bubble_controller_
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

void ChooserBubbleUiCocoa::OnOptionAdded(size_t index) {
  [chooser_bubble_ui_controller_ onOptionAdded:static_cast<NSInteger>(index)];
}

void ChooserBubbleUiCocoa::OnOptionRemoved(size_t index) {
  [chooser_bubble_ui_controller_ onOptionRemoved:static_cast<NSInteger>(index)];
}

void ChooserBubbleUiCocoa::OnBubbleClosing() {
  chooser_bubble_ui_controller_ = nil;
}
