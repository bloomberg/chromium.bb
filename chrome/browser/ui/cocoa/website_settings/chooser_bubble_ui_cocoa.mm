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
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_utils.h"
#import "chrome/browser/ui/cocoa/chooser_content_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/website_settings/chooser_bubble_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/bubble/bubble_controller.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"
#include "url/origin.h"

std::unique_ptr<BubbleUi> ChooserBubbleDelegate::BuildBubbleUi() {
  return base::WrapUnique(
      new ChooserBubbleUiCocoa(browser_, chooser_controller()));
}

@interface ChooserBubbleUiController
    : BaseBubbleController<NSTableViewDataSource, NSTableViewDelegate> {
 @private
  // Bridge to the C++ class that created this object.
  ChooserBubbleUiCocoa* bridge_;  // Weak.
  bool buttonPressed_;

  base::scoped_nsobject<ChooserContentView> chooserContentView_;
  NSTableView* tableView_;   // Weak.
  NSButton* connectButton_;  // Weak.
  NSButton* cancelButton_;   // Weak.
  NSButton* helpButton_;     // Weak.

  Browser* browser_;                      // Weak.
  ChooserController* chooserController_;  // Weak.
}

// Designated initializer.  |browser| and |bridge| must both be non-nil.
- (id)initWithBrowser:(Browser*)browser
    chooserController:(ChooserController*)chooserController
      bubbleReference:(BubbleReference)bubbleReference
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

// Called when the "Connect" button is pressed.
- (void)onConnect:(id)sender;

// Called when the "Cancel" button is pressed.
- (void)onCancel:(id)sender;

// Called when the "Get help" button is pressed.
- (void)onHelpPressed:(id)sender;

@end

@implementation ChooserBubbleUiController

- (id)initWithBrowser:(Browser*)browser
    chooserController:(ChooserController*)chooserController
      bubbleReference:(BubbleReference)bubbleReference
               bridge:(ChooserBubbleUiCocoa*)bridge {
  DCHECK(browser);
  DCHECK(chooserController);
  DCHECK(bubbleReference);
  DCHECK(bridge);

  browser_ = browser;
  chooserController_ = chooserController;

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
    self.bubbleReference = bubbleReference;
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
    chooserController_->Close();
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
  chooserContentView_.reset([[ChooserContentView alloc]
      initWithChooserTitle:
          l10n_util::GetNSStringF(
              IDS_CHOOSER_BUBBLE_PROMPT,
              url_formatter::FormatOriginForSecurityDisplay(
                  chooserController_->GetOrigin(),
                  url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC))]);

  tableView_ = [chooserContentView_ tableView];
  connectButton_ = [chooserContentView_ connectButton];
  cancelButton_ = [chooserContentView_ cancelButton];
  helpButton_ = [chooserContentView_ helpButton];

  [connectButton_ setTarget:self];
  [connectButton_ setAction:@selector(onConnect:)];
  [cancelButton_ setTarget:self];
  [cancelButton_ setAction:@selector(onCancel:)];
  [tableView_ setDelegate:self];
  [tableView_ setDataSource:self];
  [helpButton_ setTarget:self];
  [helpButton_ setAction:@selector(onHelpPressed:)];

  [[[self window] contentView] addSubview:chooserContentView_.get()];

  NSRect bubbleFrame =
      [[self window] frameRectForContentRect:[chooserContentView_ frame]];
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
    [[self window] setInitialFirstResponder:tableView_];
  }
}

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView {
  // When there are no devices, the table contains a message saying there are
  // no devices, so the number of rows is always at least 1.
  return std::max(static_cast<NSInteger>(chooserController_->NumOptions()),
                  static_cast<NSInteger>(1));
}

- (id)tableView:(NSTableView*)tableView
    objectValueForTableColumn:(NSTableColumn*)tableColumn
                          row:(NSInteger)rowIndex {
  NSInteger num_options =
      static_cast<NSInteger>(chooserController_->NumOptions());
  if (num_options == 0) {
    DCHECK_EQ(0, rowIndex);
    return l10n_util::GetNSString(IDS_CHOOSER_BUBBLE_NO_DEVICES_FOUND_PROMPT);
  }

  DCHECK_GE(rowIndex, 0);
  DCHECK_LT(rowIndex, num_options);
  return base::SysUTF16ToNSString(
      chooserController_->GetOption(static_cast<size_t>(rowIndex)));
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
  [tableView_ setEnabled:chooserController_->NumOptions() > 0];
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
  chooserController_->Select(row);
  self.bubbleReference->CloseBubble(BUBBLE_CLOSE_ACCEPTED);
  [self close];
}

- (void)onCancel:(id)sender {
  buttonPressed_ = true;
  chooserController_->Cancel();
  self.bubbleReference->CloseBubble(BUBBLE_CLOSE_CANCELED);
  [self close];
}

- (void)onHelpPressed:(id)sender {
  chooserController_->OpenHelpCenterUrl();
}

@end

ChooserBubbleUiCocoa::ChooserBubbleUiCocoa(
    Browser* browser,
    ChooserController* chooser_controller)
    : browser_(browser),
      chooser_controller_(chooser_controller),
      chooser_bubble_ui_controller_(nil) {
  DCHECK(browser);
  DCHECK(chooser_controller);
  chooser_controller_->set_observer(this);
}

ChooserBubbleUiCocoa::~ChooserBubbleUiCocoa() {
  chooser_controller_->set_observer(nullptr);
  [chooser_bubble_ui_controller_ close];
  chooser_bubble_ui_controller_ = nil;
}

void ChooserBubbleUiCocoa::Show(BubbleReference bubble_reference) {
  if (!chooser_bubble_ui_controller_) {
    chooser_bubble_ui_controller_ =
        [[ChooserBubbleUiController alloc] initWithBrowser:browser_
                                         chooserController:chooser_controller_
                                           bubbleReference:bubble_reference
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
