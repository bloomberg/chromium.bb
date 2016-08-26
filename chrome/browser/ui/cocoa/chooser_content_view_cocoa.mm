// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/chooser_content_view_cocoa.h"

#include <algorithm>

#include "base/macros.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_button.h"
#include "chrome/browser/ui/cocoa/spinner_view.h"
#include "chrome/grit/generated_resources.h"
#include "grit/ui_resources.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/controls/hyperlink_button_cell.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Chooser width.
const CGFloat kChooserWidth = 350.0f;

// Chooser height.
const CGFloat kChooserHeight = 300.0f;

// Signal strength level image size.
const CGFloat kSignalStrengthLevelImageSize = 20.0f;

// Table row view height.
const CGFloat kTableRowViewHeight = 23.0f;

// Spinner size.
const CGFloat kSpinnerSize = 24.0f;

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

// The lookup table for signal strength level image.
const int kSignalStrengthLevelImageIds[5] = {IDR_SIGNAL_0_BAR, IDR_SIGNAL_1_BAR,
                                             IDR_SIGNAL_2_BAR, IDR_SIGNAL_3_BAR,
                                             IDR_SIGNAL_4_BAR};

}  // namespace

// A table row view that contains one line of text, and optionally contains an
// image in front of the text.
@interface ChooserContentTableRowView : NSView {
 @private
  base::scoped_nsobject<NSImageView> image_;
  base::scoped_nsobject<NSTextField> text_;
}

// Designated initializer.
- (instancetype)initWithText:(NSString*)text
         signalStrengthLevel:(NSInteger)level;

// Gets the image in front of the text.
- (NSImageView*)image;

// Gets the text.
- (NSTextField*)text;

@end

@implementation ChooserContentTableRowView

- (instancetype)initWithText:(NSString*)text
         signalStrengthLevel:(NSInteger)level {
  if ((self = [super initWithFrame:NSZeroRect])) {
    if (level != -1) {
      DCHECK_GE(level, 0);
      DCHECK_LT(level, base::checked_cast<NSInteger>(
                           arraysize(kSignalStrengthLevelImageIds)));
      ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      NSImage* signalStrengthLevelImage =
          rb.GetNativeImageNamed(kSignalStrengthLevelImageIds[level])
              .ToNSImage();

      image_.reset([[NSImageView alloc]
          initWithFrame:NSMakeRect(0, (kTableRowViewHeight -
                                       kSignalStrengthLevelImageSize) /
                                          2,
                                   kSignalStrengthLevelImageSize,
                                   kSignalStrengthLevelImageSize)]);
      [image_ setImage:signalStrengthLevelImage];
      [self addSubview:image_];
    }

    text_.reset([[NSTextField alloc] initWithFrame:NSZeroRect]);
    [text_ setDrawsBackground:NO];
    [text_ setBezeled:NO];
    [text_ setEditable:NO];
    [text_ setSelectable:NO];
    [text_ setStringValue:text];
    [text_ setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];
    [text_ sizeToFit];
    CGFloat textHeight = NSHeight([text_ frame]);
    [text_ setFrameOrigin:NSMakePoint(
                              level == -1 ? 0 : kSignalStrengthLevelImageSize +
                                                    kHorizontalPadding,
                              (kTableRowViewHeight - textHeight) / 2)];
    [self addSubview:text_];
  }

  return self;
}

- (NSImageView*)image {
  return image_.get();
}

- (NSTextField*)text {
  return text_.get();
}

@end

class ChooserContentViewController : public ChooserController::View {
 public:
  ChooserContentViewController(ChooserContentViewCocoa* chooser_content_view,
                               ChooserController* chooser_controller,
                               NSTableView* table_view,
                               SpinnerView* spinner,
                               NSTextField* status,
                               NSButton* rescan_button);
  ~ChooserContentViewController() override;

  // ChooserController::View:
  void OnOptionsInitialized() override;
  void OnOptionAdded(size_t index) override;
  void OnOptionRemoved(size_t index) override;
  void OnOptionUpdated(size_t index) override;
  void OnAdapterEnabledChanged(bool enabled) override;
  void OnRefreshStateChanged(bool refreshing) override;

  void UpdateTableView();

 private:
  ChooserContentViewCocoa* chooser_content_view_;
  ChooserController* chooser_controller_;
  NSTableView* table_view_;
  SpinnerView* spinner_;
  NSTextField* status_;
  NSButton* rescan_button_;

  DISALLOW_COPY_AND_ASSIGN(ChooserContentViewController);
};

ChooserContentViewController::ChooserContentViewController(
    ChooserContentViewCocoa* chooser_content_view,
    ChooserController* chooser_controller,
    NSTableView* table_view,
    SpinnerView* spinner,
    NSTextField* status,
    NSButton* rescan_button)
    : chooser_content_view_(chooser_content_view),
      chooser_controller_(chooser_controller),
      table_view_(table_view),
      spinner_(spinner),
      status_(status),
      rescan_button_(rescan_button) {
  DCHECK(chooser_controller_);
  DCHECK(table_view_);
  DCHECK(spinner_);
  DCHECK(status_);
  DCHECK(rescan_button_);
  chooser_controller_->set_view(this);
}

ChooserContentViewController::~ChooserContentViewController() {
  chooser_controller_->set_view(nullptr);
}

void ChooserContentViewController::OnOptionsInitialized() {
  UpdateTableView();
}

void ChooserContentViewController::OnOptionAdded(size_t index) {
  UpdateTableView();
  [table_view_ setHidden:NO];
  [spinner_ setHidden:YES];
}

void ChooserContentViewController::OnOptionRemoved(size_t index) {
  // |table_view_| will automatically select the removed item's next item.
  // So here it tracks if the removed item is the item that was currently
  // selected, if so, deselect it. Also if the removed item is before the
  // currently selected item, the currently selected item's index needs to
  // be adjusted by one.
  NSInteger idx = static_cast<NSInteger>(index);
  NSInteger selected_row = [table_view_ selectedRow];
  if (selected_row == idx)
    [table_view_ deselectRow:idx];
  else if (selected_row > idx)
    [table_view_
            selectRowIndexes:[NSIndexSet indexSetWithIndex:selected_row - 1]
        byExtendingSelection:NO];

  UpdateTableView();
}

void ChooserContentViewController::OnOptionUpdated(size_t index) {
  UpdateTableView();
}

void ChooserContentViewController::OnAdapterEnabledChanged(bool enabled) {
  // No row is selected since the adapter status has changed.
  // This will also disable the OK button if it was enabled because
  // of a previously selected row.
  [table_view_ deselectAll:nil];
  UpdateTableView();
  [table_view_ setHidden:NO];

  [spinner_ setHidden:YES];

  [status_ setHidden:YES];
  // When adapter is enabled, show |rescan_button_|; otherwise hide it.
  [rescan_button_ setHidden:enabled ? NO : YES];

  [chooser_content_view_ updateView];
}

void ChooserContentViewController::OnRefreshStateChanged(bool refreshing) {
  if (refreshing) {
    // No row is selected since the chooser is refreshing.
    // This will also disable the OK button if it was enabled because
    // of a previously selected row.
    [table_view_ deselectAll:nil];
    UpdateTableView();
  }

  // When refreshing and no option available yet, hide |table_view_| and show
  // |spinner_|. Otherwise show |table_view_| and hide |spinner_|.
  bool table_view_hidden =
      refreshing && (chooser_controller_->NumOptions() == 0);
  [table_view_ setHidden:table_view_hidden ? YES : NO];
  [spinner_ setHidden:table_view_hidden ? NO : YES];

  // When refreshing, show |status_| and hide |rescan_button_|.
  // When complete, show |rescan_button_| and hide |status_|.
  [status_ setHidden:refreshing ? NO : YES];
  [rescan_button_ setHidden:refreshing ? YES : NO];

  [chooser_content_view_ updateView];
}

void ChooserContentViewController::UpdateTableView() {
  [table_view_ setEnabled:chooser_controller_->NumOptions() > 0];
  // For NSView-based table views, calling reloadData will deselect the
  // currently selected row, so |selected_row| stores the currently selected
  // row in order to select it again.
  NSInteger selected_row = [table_view_ selectedRow];
  [table_view_ reloadData];
  if (selected_row != -1) {
    [table_view_ selectRowIndexes:[NSIndexSet indexSetWithIndex:selected_row]
             byExtendingSelection:NO];
  }
}

@implementation ChooserContentViewCocoa

// TODO(juncai): restructure this function to be some smaller methods to
// create the pieces for the view. By doing so, the methods that calculate
// the frame and origins can be moved into those methods, rather than as
// helper functions.
- (instancetype)initWithChooserTitle:(NSString*)chooserTitle
                   chooserController:
                       (std::unique_ptr<ChooserController>)chooserController {
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
    chooserController_ = std::move(chooserController);

    // Create the views.
    // Title.
    titleView_ = [self createChooserTitle:chooserTitle];
    titleHeight_ = NSHeight([titleView_ frame]);

    // Status.
    status_ = [self createTextField:l10n_util::GetNSString(
                                        IDS_BLUETOOTH_DEVICE_CHOOSER_SCANNING)];
    CGFloat statusWidth = kChooserWidth / 2 - kMarginX;
    // The height is arbitrary as it will be adjusted later.
    [status_ setFrameSize:NSMakeSize(statusWidth, 0.0f)];
    [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:status_];
    statusHeight_ = NSHeight([status_ frame]);

    // Re-scan button.
    rescanButton_ =
        [self createHyperlinkButtonWithText:
                  l10n_util::GetNSString(IDS_BLUETOOTH_DEVICE_CHOOSER_RE_SCAN)];
    rescanButtonHeight_ = NSHeight([rescanButton_ frame]);

    // Connect button.
    connectButton_ = [self createConnectButton];
    connectButtonWidth_ = NSWidth([connectButton_ frame]);
    connectButtonHeight_ = NSHeight([connectButton_ frame]);

    // Cancel button.
    cancelButton_ = [self createCancelButton];
    cancelButtonWidth_ = NSWidth([cancelButton_ frame]);
    cancelButtonHeight_ = NSHeight([cancelButton_ frame]);

    CGFloat buttonRowHeight =
        std::max(connectButtonHeight_, cancelButtonHeight_);

    // Separator.
    separator_ = [self createSeparator];

    // Message.
    message_ = [self createTextField:l10n_util::GetNSStringF(
                                         IDS_DEVICE_CHOOSER_FOOTNOTE_TEXT,
                                         base::string16())];
    CGFloat messageWidth = NSWidth([message_ frame]);
    messageHeight_ = NSHeight([message_ frame]);

    // Help button.
    helpButton_ = [self
        createHyperlinkButtonWithText:
            l10n_util::GetNSString(IDS_DEVICE_CHOOSER_GET_HELP_LINK_TEXT)];

    // ScollView embedding with TableView.
    noStatusOrRescanButtonShown_.scroll_view_frame =
        [self calculateScrollViewFrame:buttonRowHeight];
    scrollView_.reset([[NSScrollView alloc]
        initWithFrame:noStatusOrRescanButtonShown_.scroll_view_frame]);
    [scrollView_ setBorderType:NSBezelBorder];
    [scrollView_ setHasVerticalScroller:YES];
    [scrollView_ setHasHorizontalScroller:YES];
    [scrollView_ setAutohidesScrollers:YES];

    // TableView.
    tableView_.reset([[NSTableView alloc] initWithFrame:NSZeroRect]);
    tableColumn_.reset([[NSTableColumn alloc] initWithIdentifier:@""]);
    [tableColumn_
        setWidth:(noStatusOrRescanButtonShown_.scroll_view_frame.size.width -
                  kMarginX)];
    [tableView_ addTableColumn:tableColumn_];
    // Make the column title invisible.
    [tableView_ setHeaderView:nil];
    [tableView_ setFocusRingType:NSFocusRingTypeNone];

    // Spinner.
    // Set the spinner in the center of the scroll view.
    // When |status_| is shown, it may affect the frame origin and size of the
    // |scrollView_|, and since the |spinner_| is shown with the |status_|,
    // its frame origin needs to be calculated according to the frame origin
    // of |scrollView_| with |status_| shown.
    NSRect scrollViewFrameWithStatusText = [self
        calculateScrollViewFrame:std::max(statusHeight_, buttonRowHeight)];
    CGFloat spinnerOriginX =
        scrollViewFrameWithStatusText.origin.x +
        (scrollViewFrameWithStatusText.size.width - kSpinnerSize) / 2;
    CGFloat spinnerOriginY =
        scrollViewFrameWithStatusText.origin.y +
        (scrollViewFrameWithStatusText.size.height - kSpinnerSize) / 2;
    spinner_.reset([[SpinnerView alloc]
        initWithFrame:NSMakeRect(spinnerOriginX, spinnerOriginY, kSpinnerSize,
                                 kSpinnerSize)]);

    // Lay out the views.
    // Title.
    CGFloat titleOriginX = kMarginX;
    CGFloat titleOriginY = kChooserHeight - kMarginY - titleHeight_;
    [titleView_ setFrameOrigin:NSMakePoint(titleOriginX, titleOriginY)];
    [self addSubview:titleView_];

    // ScollView and Spinner. Only one of them is shown.
    [scrollView_ setDocumentView:tableView_];
    [self addSubview:scrollView_];
    [spinner_ setHidden:YES];
    [self addSubview:spinner_];

    // Status text field and Re-scan button. At most one of them is shown.
    [self addSubview:status_];
    [status_ setHidden:YES];

    [rescanButton_ setTarget:self];
    [rescanButton_ setAction:@selector(onRescan:)];
    [self addSubview:rescanButton_];
    [rescanButton_ setHidden:YES];

    // Connect button.
    noStatusOrRescanButtonShown_.connect_button_origin =
        [self calculateConnectButtonOrigin:buttonRowHeight];
    [connectButton_
        setFrameOrigin:noStatusOrRescanButtonShown_.connect_button_origin];
    [connectButton_ setEnabled:NO];
    [self addSubview:connectButton_];

    // Cancel button.
    noStatusOrRescanButtonShown_.cancel_button_origin =
        [self calculateCancelButtonOrigin:buttonRowHeight];
    [cancelButton_
        setFrameOrigin:noStatusOrRescanButtonShown_.cancel_button_origin];
    [self addSubview:cancelButton_];

    // Separator.
    CGFloat separatorOriginX = 0.0f;
    CGFloat separatorOriginY = kMarginY + messageHeight_ + kVerticalPadding;
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
    [helpButton_ setTarget:self];
    [helpButton_ setAction:@selector(onHelpPressed:)];
    [self addSubview:helpButton_];

    // Calculate and cache the frame and origins values.
    buttonRowHeight = std::max(
        statusHeight_, std::max(connectButtonHeight_, cancelButtonHeight_));
    statusShown_ = {[self calculateScrollViewFrame:buttonRowHeight],
                    [self calculateConnectButtonOrigin:buttonRowHeight],
                    [self calculateCancelButtonOrigin:buttonRowHeight]};
    statusOrigin_ = [self calculateStatusOrigin:buttonRowHeight];

    buttonRowHeight =
        std::max(rescanButtonHeight_,
                 std::max(connectButtonHeight_, cancelButtonHeight_));
    rescanButtonShown_ = {[self calculateScrollViewFrame:buttonRowHeight],
                          [self calculateConnectButtonOrigin:buttonRowHeight],
                          [self calculateCancelButtonOrigin:buttonRowHeight]};
    rescanButtonOrigin_ = [self calculateRescanButtonOrigin:buttonRowHeight];

    chooserContentViewController_.reset(new ChooserContentViewController(
        self, chooserController_.get(), tableView_.get(), spinner_.get(),
        status_.get(), rescanButton_.get()));
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

- (base::scoped_nsobject<NSView>)createTableRowView:(NSInteger)rowIndex {
  NSInteger level = -1;
  size_t numOptions = chooserController_->NumOptions();
  if (chooserController_->ShouldShowIconBeforeText() && numOptions > 0) {
    DCHECK_GE(rowIndex, 0);
    DCHECK_LT(rowIndex, base::checked_cast<NSInteger>(numOptions));
    level = base::checked_cast<NSInteger>(
        chooserController_->GetSignalStrengthLevel(
            base::checked_cast<size_t>(rowIndex)));
  }

  base::scoped_nsobject<NSView> tableRowView([[ChooserContentTableRowView alloc]
             initWithText:[self optionAtIndex:rowIndex]
      signalStrengthLevel:level]);
  return tableRowView;
}

- (CGFloat)tableRowViewHeight:(NSInteger)row {
  return kTableRowViewHeight;
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
      base::SysUTF16ToNSString(chooserController_->GetOkButtonLabel());
  return [self createButtonWithTitle:connectTitle];
}

- (base::scoped_nsobject<NSButton>)createCancelButton {
  NSString* cancelTitle =
      l10n_util::GetNSString(IDS_DEVICE_CHOOSER_CANCEL_BUTTON_TEXT);
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

- (base::scoped_nsobject<NSTextField>)createTextField:(NSString*)text {
  base::scoped_nsobject<NSTextField> textField(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  [textField setDrawsBackground:NO];
  [textField setBezeled:NO];
  [textField setEditable:NO];
  [textField setSelectable:NO];
  [textField setStringValue:text];
  [textField setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]];
  [textField sizeToFit];
  return textField;
}

- (base::scoped_nsobject<NSButton>)createHyperlinkButtonWithText:
    (NSString*)text {
  base::scoped_nsobject<NSButton> button(
      [[NSButton alloc] initWithFrame:NSZeroRect]);
  base::scoped_nsobject<HyperlinkButtonCell> cell(
      [[HyperlinkButtonCell alloc] initTextCell:text]);
  [button setCell:cell.get()];
  [button sizeToFit];
  return button;
}

- (NSRect)calculateScrollViewFrame:(CGFloat)buttonRowHeight {
  CGFloat originX = kMarginX;
  CGFloat originY =
      kMarginY + messageHeight_ + 3 * kVerticalPadding + buttonRowHeight;
  CGFloat width = kChooserWidth - 2 * kMarginX;
  CGFloat height = kChooserHeight - 2 * kMarginY - 4 * kVerticalPadding -
                   titleHeight_ - buttonRowHeight - messageHeight_;
  return NSMakeRect(originX, originY, width, height);
}

- (NSPoint)calculateStatusOrigin:(CGFloat)buttonRowHeight {
  return NSMakePoint(kMarginX, kMarginY + messageHeight_ +
                                   2 * kVerticalPadding +
                                   (buttonRowHeight - statusHeight_) / 2);
}

- (NSPoint)calculateRescanButtonOrigin:(CGFloat)buttonRowHeight {
  return NSMakePoint(kMarginX, kMarginY + messageHeight_ +
                                   2 * kVerticalPadding +
                                   (buttonRowHeight - rescanButtonHeight_) / 2);
}

- (NSPoint)calculateConnectButtonOrigin:(CGFloat)buttonRowHeight {
  return NSMakePoint(kChooserWidth - kMarginX - kHorizontalPadding -
                         connectButtonWidth_ - cancelButtonWidth_,
                     kMarginY + messageHeight_ + 2 * kVerticalPadding +
                         (buttonRowHeight - connectButtonHeight_) / 2);
}

- (NSPoint)calculateCancelButtonOrigin:(CGFloat)buttonRowHeight {
  return NSMakePoint(kChooserWidth - kMarginX - cancelButtonWidth_,
                     kMarginY + messageHeight_ + 2 * kVerticalPadding +
                         (buttonRowHeight - cancelButtonHeight_) / 2);
}

- (void)updateView {
  FrameAndOrigin frameAndOrigin;
  if (![status_ isHidden]) {
    [status_ setFrameOrigin:statusOrigin_];
    frameAndOrigin = statusShown_;
  } else if (![rescanButton_ isHidden]) {
    [rescanButton_ setFrameOrigin:rescanButtonOrigin_];
    frameAndOrigin = rescanButtonShown_;
  } else {
    frameAndOrigin = noStatusOrRescanButtonShown_;
  }

  [scrollView_ setFrame:frameAndOrigin.scroll_view_frame];
  [connectButton_ setFrameOrigin:frameAndOrigin.connect_button_origin];
  [cancelButton_ setFrameOrigin:frameAndOrigin.cancel_button_origin];
}

- (NSTableView*)tableView {
  return tableView_.get();
}

- (SpinnerView*)spinner {
  return spinner_.get();
}

- (NSTextField*)status {
  return status_.get();
}

- (NSButton*)rescanButton {
  return rescanButton_.get();
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

- (NSInteger)numberOfOptions {
  // When there are no devices, the table contains a message saying there are
  // no devices, so the number of rows is always at least 1.
  return std::max(static_cast<NSInteger>(chooserController_->NumOptions()),
                  static_cast<NSInteger>(1));
}

// When this function is called with numOptions == 0, it is to show the
// message saying there are no devices.
- (NSString*)optionAtIndex:(NSInteger)index {
  NSInteger numOptions =
      static_cast<NSInteger>(chooserController_->NumOptions());
  if (numOptions == 0) {
    DCHECK_EQ(0, index);
    return base::SysUTF16ToNSString(chooserController_->GetNoOptionsText());
  }

  DCHECK_GE(index, 0);
  DCHECK_LT(index, numOptions);

  return base::SysUTF16ToNSString(
      chooserController_->GetOption(static_cast<size_t>(index)));
}

- (void)updateTableView {
  chooserContentViewController_->UpdateTableView();
}

- (void)accept {
  chooserController_->Select([tableView_ selectedRow]);
}

- (void)cancel {
  chooserController_->Cancel();
}

- (void)close {
  chooserController_->Close();
}

- (void)onRescan:(id)sender {
  chooserController_->RefreshOptions();
}

- (void)onHelpPressed:(id)sender {
  chooserController_->OpenHelpCenterUrl();
}

- (NSImageView*)tableRowViewImage:(NSInteger)row {
  ChooserContentTableRowView* tableRowView =
      [tableView_ viewAtColumn:0 row:row makeIfNecessary:YES];
  return [tableRowView image];
}

- (NSTextField*)tableRowViewText:(NSInteger)row {
  ChooserContentTableRowView* tableRowView =
      [tableView_ viewAtColumn:0 row:row makeIfNecessary:YES];
  return [tableRowView text];
}

@end
