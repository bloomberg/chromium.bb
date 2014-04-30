// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/extensions/media_galleries_scan_result_dialog_cocoa.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_alert.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_control_utils.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"
#import "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#import "ui/base/cocoa/flipped_view.h"
#import "ui/base/models/menu_model.h"
#include "ui/base/l10n/l10n_util.h"

const CGFloat kCheckboxLeading = 10;
const CGFloat kCheckboxWidth = 440;
const CGFloat kScrollAreaHeight = 220;

// Controller for UI events on items in the media galleries dialog.
@interface MediaGalleriesScanResultCocoaController : NSObject {
 @private
  MediaGalleriesScanResultDialogCocoa* dialog_;
}

@property(assign, nonatomic) MediaGalleriesScanResultDialogCocoa* dialog;

- (void)onAcceptButton:(id)sender;
- (void)onCancelButton:(id)sender;

@end

@implementation MediaGalleriesScanResultCocoaController

@synthesize dialog = dialog_;

- (void)onAcceptButton:(id)sender {
  dialog_->OnAcceptClicked();
}

- (void)onCancelButton:(id)sender {
  dialog_->OnCancelClicked();
}

@end


MediaGalleriesScanResultDialogCocoa::MediaGalleriesScanResultDialogCocoa(
    MediaGalleriesScanResultDialogController* controller,
    MediaGalleriesScanResultCocoaController* cocoa_controller)
    : controller_(controller),
      accepted_(false),
      cocoa_controller_([cocoa_controller retain]) {
  [cocoa_controller_ setDialog:this];

  alert_.reset([[ConstrainedWindowAlert alloc] init]);

  [alert_ setMessageText:base::SysUTF16ToNSString(controller_->GetHeader())];
  [alert_ setInformativeText:
      base::SysUTF16ToNSString(controller_->GetSubtext())];
  [alert_ addButtonWithTitle:
      l10n_util::GetNSString(IDS_MEDIA_GALLERIES_SCAN_RESULT_DIALOG_CONFIRM)
               keyEquivalent:kKeyEquivalentReturn
                      target:cocoa_controller_
                      action:@selector(onAcceptButton:)];
  [alert_ addButtonWithTitle:
      l10n_util::GetNSString(IDS_MEDIA_GALLERIES_DIALOG_CANCEL)
               keyEquivalent:kKeyEquivalentEscape
                      target:cocoa_controller_
                      action:@selector(onCancelButton:)];
  [[alert_ closeButton] setTarget:cocoa_controller_];
  [[alert_ closeButton] setAction:@selector(onCancelButton:)];

  InitDialogControls();

  // May be NULL during tests.
  if (controller->web_contents()) {
    base::scoped_nsobject<CustomConstrainedWindowSheet> sheet(
        [[CustomConstrainedWindowSheet alloc]
            initWithCustomWindow:[alert_ window]]);
    window_.reset(new ConstrainedWindowMac(
        this, controller->web_contents(), sheet));
  }
}

MediaGalleriesScanResultDialogCocoa::~MediaGalleriesScanResultDialogCocoa() {
}

void MediaGalleriesScanResultDialogCocoa::InitDialogControls() {
  main_container_.reset([[NSBox alloc] init]);
  [main_container_ setBoxType:NSBoxCustom];
  [main_container_ setBorderType:NSLineBorder];
  [main_container_ setBorderWidth:1];
  [main_container_ setCornerRadius:0];
  [main_container_ setContentViewMargins:NSZeroSize];
  [main_container_ setTitlePosition:NSNoTitle];
  [main_container_ setBorderColor:[NSColor disabledControlTextColor]];

  base::scoped_nsobject<NSScrollView> scroll_view(
      [[NSScrollView alloc] initWithFrame:
          NSMakeRect(0, 0, kCheckboxWidth, kScrollAreaHeight)]);
  [scroll_view setHasVerticalScroller:YES];
  [scroll_view setHasHorizontalScroller:NO];
  [scroll_view setBorderType:NSNoBorder];
  [scroll_view setAutohidesScrollers:YES];
  [[main_container_ contentView] addSubview:scroll_view];

  // Add scan results checkboxes inside the scrolling view.
  checkbox_container_.reset([[FlippedView alloc] initWithFrame:NSZeroRect]);
  CGFloat height = CreateCheckboxes();
  // Give the container a reasonable initial size so that the scroll_view can
  // figure out the content size.
  [checkbox_container_ setFrameSize:NSMakeSize(kCheckboxWidth, height)];
  [scroll_view setDocumentView:checkbox_container_];
  [checkbox_container_ setFrameSize:NSMakeSize([scroll_view contentSize].width,
                                               height)];

  // Resize to pack the scroll view if possible.
  NSRect scroll_frame = [scroll_view frame];
  if (NSHeight(scroll_frame) > NSHeight([checkbox_container_ frame])) {
    scroll_frame.size.height = NSHeight([checkbox_container_ frame]);
    [scroll_view setFrameSize:scroll_frame.size];
  }

  [main_container_ setFrameFromContentFrame:scroll_frame];
  [main_container_ setFrameOrigin:NSZeroPoint];
  [alert_ setAccessoryView:main_container_];
  [alert_ layout];
}

void MediaGalleriesScanResultDialogCocoa::AcceptDialogForTesting() {
  OnAcceptClicked();
}

CGFloat MediaGalleriesScanResultDialogCocoa::CreateCheckboxes() {
  CGFloat y_pos = 0;
  MediaGalleriesScanResultDialogController::OrderedScanResults scan_results =
      controller_->GetGalleryList();
  for (MediaGalleriesScanResultDialogController::OrderedScanResults::
       const_iterator iter = scan_results.begin();
       iter != scan_results.end(); iter++) {
    const MediaGalleriesScanResultDialogController::ScanResult& scan_result =
        *iter;
    base::scoped_nsobject<MediaGalleryListEntry> checkbox_entry(
        [[MediaGalleryListEntry alloc]
            initWithFrame:NSZeroRect
               controller:this
                   prefId:scan_result.pref_info.pref_id
              galleryName:scan_result.pref_info.GetGalleryDisplayName()
                subscript:scan_result.pref_info.GetGalleryAdditionalDetails()
                  tooltip:scan_result.pref_info.GetGalleryTooltip()
         showFolderViewer:scan_result.pref_info.IsGalleryAvailable()]);

    [checkbox_entry setState:scan_result.selected];

    [checkbox_entry setFrameOrigin:NSMakePoint(0, y_pos)];
    y_pos = NSMaxY([checkbox_entry frame]) + kCheckboxLeading;

    [checkbox_container_ addSubview:checkbox_entry];
  }

  return y_pos;
}

void MediaGalleriesScanResultDialogCocoa::OnAcceptClicked() {
  accepted_ = true;

  if (window_)
    window_->CloseWebContentsModalDialog();
}

void MediaGalleriesScanResultDialogCocoa::OnCancelClicked() {
  if (window_)
    window_->CloseWebContentsModalDialog();
}

void MediaGalleriesScanResultDialogCocoa::UpdateResults() {
  InitDialogControls();
}

void MediaGalleriesScanResultDialogCocoa::OnConstrainedWindowClosed(
    ConstrainedWindowMac* window) {
  controller_->DialogFinished(accepted_);
}

void MediaGalleriesScanResultDialogCocoa::OnCheckboxToggled(
    MediaGalleryPrefId prefId, bool checked) {
  controller_->DidToggleGalleryId(prefId, checked);
}

void MediaGalleriesScanResultDialogCocoa::OnFolderViewerClicked(
    MediaGalleryPrefId prefId) {
  controller_->DidClickOpenFolderViewer(prefId);
}

ui::MenuModel* MediaGalleriesScanResultDialogCocoa::GetContextMenu(
    MediaGalleryPrefId pref_id) {
  return controller_->GetContextMenu(pref_id);
}

// static
MediaGalleriesScanResultDialog* MediaGalleriesScanResultDialog::Create(
    MediaGalleriesScanResultDialogController* controller) {
  base::scoped_nsobject<MediaGalleriesScanResultCocoaController>
      cocoa_controller([[MediaGalleriesScanResultCocoaController alloc] init]);
  return new MediaGalleriesScanResultDialogCocoa(controller, cocoa_controller);
}
