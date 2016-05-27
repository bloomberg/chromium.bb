// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/chooser_content_view.h"

#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_button.h"
#include "chrome/grit/generated_resources.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/controls/hyperlink_button_cell.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

// Chooser width.
const CGFloat kChooserWidth = 320.0f;

// Chooser height.
const CGFloat kChooserHeight = 280.0f;

// Distance between the chooser border and the view that is closest to the
// border.
const CGFloat kMarginX = 20.0f;
const CGFloat kMarginY = 20.0f;

// Distance between two views inside the chooser.
const CGFloat kHorizontalPadding = 10.0f;
const CGFloat kVerticalPadding = 10.0f;

// Separator alpha value.
const CGFloat kSeparatorAlphaValue = 0.6f;

// Separator height.
const CGFloat kSeparatorHeight = 1.0f;

}  // namespace

@implementation ChooserContentView

- (instancetype)initWithChooserTitle:(NSString*)chooserTitle {
  // ------------------------------------
  // | Chooser title                    |
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

  // Determine the dimensions of the chooser.
  // Once the height and width are set, the buttons and permission menus can
  // be laid out correctly.
  NSRect chooserFrame = NSMakeRect(0, 0, kChooserWidth, kChooserHeight);

  if ((self = [super initWithFrame:chooserFrame])) {
    // Create the views.
    // Title.
    titleView_ = [self createChooserTitle:chooserTitle];
    CGFloat titleHeight = NSHeight([titleView_ frame]);

    // Connect button.
    connectButton_ = [self createConnectButton];
    CGFloat connectButtonWidth = NSWidth([connectButton_ frame]);
    CGFloat connectButtonHeight = NSHeight([connectButton_ frame]);

    // Cancel button.
    cancelButton_ = [self createCancelButton];
    CGFloat cancelButtonWidth = NSWidth([cancelButton_ frame]);

    // Separator.
    separator_ = [self createSeparator];

    // Message.
    message_ = [self createMessage];
    CGFloat messageWidth = NSWidth([message_ frame]);
    CGFloat messageHeight = NSHeight([message_ frame]);

    // Help button.
    helpButton_ = [self createHelpButton];

    // ScollView embedding with TableView.
    CGFloat scrollViewWidth = kChooserWidth - 2 * kMarginX;
    CGFloat scrollViewHeight = kChooserHeight - 2 * kMarginY -
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
    // Make the column title invisible.
    [tableView_ setHeaderView:nil];
    [tableView_ setFocusRingType:NSFocusRingTypeNone];

    // Lay out the views.
    // Title.
    CGFloat titleOriginX = kMarginX;
    CGFloat titleOriginY = kChooserHeight - kMarginY - titleHeight;
    [titleView_ setFrameOrigin:NSMakePoint(titleOriginX, titleOriginY)];
    [self addSubview:titleView_];

    // ScollView.
    [scrollView_ setDocumentView:tableView_];
    [self addSubview:scrollView_];

    // Connect button.
    CGFloat connectButtonOriginX = kChooserWidth - kMarginX -
                                   kHorizontalPadding - connectButtonWidth -
                                   cancelButtonWidth;
    CGFloat connectButtonOriginY =
        kMarginY + messageHeight + 2 * kVerticalPadding;
    [connectButton_
        setFrameOrigin:NSMakePoint(connectButtonOriginX, connectButtonOriginY)];
    [connectButton_ setEnabled:NO];
    [self addSubview:connectButton_];

    // Cancel button.
    CGFloat cancelButtonOriginX = kChooserWidth - kMarginX - cancelButtonWidth;
    CGFloat cancelButtonOriginY = connectButtonOriginY;
    [cancelButton_
        setFrameOrigin:NSMakePoint(cancelButtonOriginX, cancelButtonOriginY)];
    [self addSubview:cancelButton_];

    // Separator.
    CGFloat separatorOriginX = 0.0f;
    CGFloat separatorOriginY = kMarginY + messageHeight + kVerticalPadding;
    [separator_ setFrameOrigin:NSMakePoint(separatorOriginX, separatorOriginY)];
    [self addSubview:separator_];

    // Message.
    CGFloat messageOriginX = kMarginX;
    CGFloat messageOriginY = kMarginY;
    [message_ setFrameOrigin:NSMakePoint(messageOriginX, messageOriginY)];
    [self addSubview:message_];

    // Help button.
    CGFloat helpButtonOriginX =
        kMarginX + messageWidth - kHorizontalPadding / 2;
    CGFloat helpButtonOriginY = kMarginY;
    [helpButton_
        setFrameOrigin:NSMakePoint(helpButtonOriginX, helpButtonOriginY)];
    [self addSubview:helpButton_];
  }

  return self;
}

- (base::scoped_nsobject<NSTextField>)createChooserTitle:(NSString*)title {
  base::scoped_nsobject<NSTextField> titleView(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  [titleView setDrawsBackground:NO];
  [titleView setBezeled:NO];
  [titleView setEditable:NO];
  [titleView setSelectable:NO];
  [titleView setStringValue:title];
  [titleView setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];
  // The height is arbitrary as it will be adjusted later.
  [titleView setFrameSize:NSMakeSize(kChooserWidth - 2 * kMarginX, 0.0f)];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:titleView];
  return titleView;
}

- (base::scoped_nsobject<NSButton>)createButtonWithTitle:(NSString*)title {
  base::scoped_nsobject<NSButton> button(
      [[ConstrainedWindowButton alloc] initWithFrame:NSZeroRect]);
  [button setButtonType:NSMomentaryPushInButton];
  [button setTitle:title];
  [button sizeToFit];
  return button;
}

- (base::scoped_nsobject<NSButton>)createConnectButton {
  NSString* connectTitle =
      l10n_util::GetNSString(IDS_CHOOSER_BUBBLE_CONNECT_BUTTON_TEXT);
  return [self createButtonWithTitle:connectTitle];
}

- (base::scoped_nsobject<NSButton>)createCancelButton {
  NSString* cancelTitle =
      l10n_util::GetNSString(IDS_CHOOSER_BUBBLE_CANCEL_BUTTON_TEXT);
  return [self createButtonWithTitle:cancelTitle];
}

- (base::scoped_nsobject<NSBox>)createSeparator {
  base::scoped_nsobject<NSBox> spacer([[NSBox alloc] initWithFrame:NSZeroRect]);
  [spacer setBoxType:NSBoxSeparator];
  [spacer setBorderType:NSLineBorder];
  [spacer setAlphaValue:kSeparatorAlphaValue];
  [spacer setFrameSize:NSMakeSize(kChooserWidth, kSeparatorHeight)];
  return spacer;
}

- (base::scoped_nsobject<NSTextField>)createMessage {
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

- (base::scoped_nsobject<NSButton>)createHelpButton {
  base::scoped_nsobject<NSButton> button(
      [[NSButton alloc] initWithFrame:NSZeroRect]);
  base::scoped_nsobject<HyperlinkButtonCell> cell([[HyperlinkButtonCell alloc]
      initTextCell:l10n_util::GetNSString(
                       IDS_CHOOSER_BUBBLE_GET_HELP_LINK_TEXT)]);
  [button setCell:cell.get()];
  [button sizeToFit];
  return button;
}

- (NSTableView*)tableView {
  return tableView_.get();
}

- (NSButton*)connectButton {
  return connectButton_.get();
}

- (NSButton*)cancelButton {
  return cancelButton_.get();
}

- (NSButton*)helpButton {
  return helpButton_.get();
}

@end
