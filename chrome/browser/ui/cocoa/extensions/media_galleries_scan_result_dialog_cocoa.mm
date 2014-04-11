// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/extensions/media_galleries_scan_result_dialog_cocoa.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_alert.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_control_utils.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"
#import "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#import "ui/base/cocoa/flipped_view.h"
#import "ui/base/cocoa/menu_controller.h"
#import "ui/base/models/menu_model.h"
#include "ui/base/l10n/l10n_util.h"

const CGFloat kDetailGray = 0.625;

// Controller for UI events on items in the media galleries dialog.
@interface MediaGalleriesScanResultCocoaController : NSObject {
 @private
  MediaGalleriesScanResultDialogCocoa* dialog_;
}

@property(assign, nonatomic) MediaGalleriesScanResultDialogCocoa* dialog;

@end

@implementation MediaGalleriesScanResultCocoaController

@synthesize dialog = dialog_;

- (void)onAcceptButton:(id)sender {
  dialog_->OnAcceptClicked();
}

- (void)onCancelButton:(id)sender {
  dialog_->OnCancelClicked();
}

- (void)onCheckboxToggled:(id)sender {
  DCHECK(dialog_);
  dialog_->OnCheckboxToggled(sender);
}

- (void)onFolderViewClicked:(id)sender {
  DCHECK(dialog_);
  dialog_->OnFolderViewClicked(sender);
}

@end


@interface MediaGalleriesScanResultButton : NSButton {
 @private
  MediaGalleriesScanResultDialogCocoa* dialog_;  // |dialog_| owns |this|.
  MediaGalleryPrefId prefId_;
  base::scoped_nsobject<MenuController> menuController_;
}

@property(readonly, nonatomic) MediaGalleryPrefId prefId;

- (id)initWithFrame:(NSRect)frameRect
             dialog:(MediaGalleriesScanResultDialogCocoa*)dialog
             prefId:(MediaGalleryPrefId)prefId;
- (NSMenu*)menuForEvent:(NSEvent*)theEvent;

@end

@implementation MediaGalleriesScanResultButton

@synthesize prefId = prefId_;

- (id)initWithFrame:(NSRect)frameRect
             dialog:(MediaGalleriesScanResultDialogCocoa*)dialog
             prefId:(MediaGalleryPrefId)prefId {
  if ((self = [super initWithFrame:frameRect])) {
    dialog_ = dialog;
    prefId_ = prefId;
  }
  return self;
}

- (NSMenu*)menuForEvent:(NSEvent*)theEvent {
  menuController_.reset(
    [[MenuController alloc] initWithModel:dialog_->GetContextMenu(prefId_)
                   useWithPopUpButtonCell:NO]);
  return [menuController_ menu];
}

@end

namespace {

const CGFloat kCheckboxMargin = 10;
const CGFloat kCheckboxMaxWidth = 440;
const CGFloat kScrollAreaHeight = 220;

}  // namespace

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
  [main_container_ setBorderColor:[NSColor colorWithCalibratedRed:kDetailGray
                                                            green:kDetailGray
                                                             blue:kDetailGray
                                                            alpha:1.0]];

  base::scoped_nsobject<NSScrollView> scroll_view(
      [[NSScrollView alloc] initWithFrame:
          NSMakeRect(0, 0, kCheckboxMaxWidth, kScrollAreaHeight)]);
  [scroll_view setHasVerticalScroller:YES];
  [scroll_view setHasHorizontalScroller:NO];
  [scroll_view setBorderType:NSNoBorder];
  [scroll_view setAutohidesScrollers:YES];
  [[main_container_ contentView] addSubview:scroll_view];

  // Add scan results checkboxes inside the scrolling view.
  checkbox_container_.reset([[FlippedView alloc] initWithFrame:NSZeroRect]);
  checkboxes_.reset([[NSMutableArray alloc] init]);
  [scroll_view setDocumentView:checkbox_container_];

  CGFloat y_pos = 0;

  y_pos = CreateCheckboxes(y_pos, controller_->GetGalleryList());

  [checkbox_container_ setFrame:NSMakeRect(0, 0, kCheckboxMaxWidth, y_pos + 2)];

  // Resize to pack the scroll view if possible.
  NSRect scroll_frame = [scroll_view frame];
  if (NSHeight(scroll_frame) > NSHeight([checkbox_container_ frame])) {
    scroll_frame.size.height = NSHeight([checkbox_container_ frame]);
    [scroll_view setFrame:scroll_frame];
  }

  [main_container_ setFrameFromContentFrame:scroll_frame];
  [alert_ setAccessoryView:main_container_];

  [alert_ layout];
}

void MediaGalleriesScanResultDialogCocoa::AcceptDialogForTesting() {
  OnAcceptClicked();
}

CGFloat MediaGalleriesScanResultDialogCocoa::CreateCheckboxes(
    CGFloat y_pos,
    const MediaGalleriesScanResultDialogController::OrderedScanResults&
        scan_results) {
  y_pos += kCheckboxMargin;

  for (MediaGalleriesScanResultDialogController::OrderedScanResults::
       const_iterator iter = scan_results.begin();
       iter != scan_results.end(); iter++) {
    const MediaGalleriesScanResultDialogController::ScanResult& scan_result =
        *iter;
    UpdateScanResultCheckbox(scan_result.pref_info, scan_result.selected,
                             y_pos);
    y_pos = NSMaxY([[checkboxes_ lastObject] frame]) + kCheckboxMargin;
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

void MediaGalleriesScanResultDialogCocoa::OnCheckboxToggled(
    NSButton* button) {
  [[[alert_ buttons] objectAtIndex:0] setEnabled:YES];

  MediaGalleriesScanResultButton* checkbox =
      (MediaGalleriesScanResultButton*) button;
  controller_->DidToggleGalleryId([checkbox prefId],
                                  [checkbox state] == NSOnState);
}

void MediaGalleriesScanResultDialogCocoa::OnFolderViewClicked(
    NSButton* button) {
  MediaGalleriesScanResultButton* checkbox =
    base::mac::ObjCCastStrict<MediaGalleriesScanResultButton>(button);
  controller_->DidClickOpenFolderViewer([checkbox prefId]);
}

void MediaGalleriesScanResultDialogCocoa::UpdateScanResultCheckbox(
    const MediaGalleryPrefInfo& gallery,
    bool selected,
    CGFloat y_pos) {
  // Checkbox.
  base::scoped_nsobject<MediaGalleriesScanResultButton> checkbox(
      [[MediaGalleriesScanResultButton alloc] initWithFrame:NSZeroRect
                                                     dialog:this
                                                     prefId:gallery.pref_id]);
  [[checkbox cell] setLineBreakMode:NSLineBreakByTruncatingMiddle];
  [checkbox setButtonType:NSSwitchButton];
  [checkbox setTarget:cocoa_controller_];
  [checkbox setAction:@selector(onCheckboxToggled:)];
  [checkboxes_ addObject:checkbox];

  [checkbox setTitle:base::SysUTF16ToNSString(
      gallery.GetGalleryDisplayName())];
  [checkbox setToolTip:base::SysUTF16ToNSString(gallery.GetGalleryTooltip())];
  [checkbox setState:selected ? NSOnState : NSOffState];
  [checkbox sizeToFit];
  NSRect checkbox_rect = [checkbox bounds];

  // Folder viewer button.
  NSRect folder_viewer_rect = NSZeroRect;
  base::scoped_nsobject<MediaGalleriesScanResultButton> folder_viewer;
  if (gallery.IsGalleryAvailable()) {
    folder_viewer.reset(
        [[MediaGalleriesScanResultButton alloc] initWithFrame:NSZeroRect
                                               dialog:this
                                               prefId:gallery.pref_id]);
    [folder_viewer setButtonType:NSMomentaryChangeButton];
    [folder_viewer setTarget:cocoa_controller_];
    [folder_viewer setAction:@selector(onFolderViewClicked:)];

    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    [folder_viewer setImage:rb.GetNativeImageNamed(
        IDR_FILE_FOLDER).ToNSImage()];
    [folder_viewer setTitle:nil];
    [folder_viewer setBordered:false];
    [folder_viewer setToolTip:
        base::SysUTF16ToNSString(gallery.GetGalleryTooltip())];
    [folder_viewer sizeToFit];
    folder_viewer_rect = [folder_viewer bounds];
  } else {
    folder_viewer_rect.size.width = -kCheckboxMargin;
  }

  // Additional details text.
  base::scoped_nsobject<NSTextField> details(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  [[details cell] setLineBreakMode:NSLineBreakByTruncatingHead];
  [details setEditable:NO];
  [details setSelectable:NO];
  [details setBezeled:NO];
  [details setAttributedStringValue:
      constrained_window::GetAttributedLabelString(
          base::SysUTF16ToNSString(gallery.GetGalleryAdditionalDetails()),
          chrome_style::kTextFontStyle,
          NSNaturalTextAlignment,
          NSLineBreakByClipping
      )];
  [details setTextColor:[NSColor colorWithCalibratedRed:kDetailGray
                                                  green:kDetailGray
                                                   blue:kDetailGray
                                                  alpha:1.0]];
  [details sizeToFit];
  NSRect details_rect = [details bounds];

  // Size the views. If all the elements don't naturally fit, the checkbox
  // should get squished and will elide in the middle.  However, it shouldn't
  // squish too much so it gets at least half of the max width and the details
  // text should elide as well in that case.
  int natural_width = NSWidth(checkbox_rect) + NSWidth(folder_viewer_rect) +
                      NSWidth(details_rect) + 5 * kCheckboxMargin;
  if (natural_width > kCheckboxMaxWidth) {
    int max_content = kCheckboxMaxWidth - 5 * kCheckboxMargin;
    if (NSWidth(folder_viewer_rect) + NSWidth(details_rect) > max_content / 2) {
      details_rect.size.width = std::max(
          max_content / 2 - NSWidth(folder_viewer_rect),
          max_content - NSWidth(checkbox_rect) - NSWidth(folder_viewer_rect));
    }
    checkbox_rect.size.width =
        max_content - NSWidth(folder_viewer_rect) - NSWidth(details_rect);
  }

  checkbox_rect.origin = NSMakePoint(kCheckboxMargin, y_pos);
  [checkbox setFrame:checkbox_rect];
  [checkbox_container_ addSubview:checkbox];

  folder_viewer_rect.origin =
      NSMakePoint(NSMaxX(checkbox_rect) + kCheckboxMargin, y_pos);
  if (gallery.IsGalleryAvailable()) {
    [folder_viewer setFrame:folder_viewer_rect];
    [checkbox_container_ addSubview:folder_viewer];
  }

  details_rect.origin =
      NSMakePoint(NSMaxX(folder_viewer_rect) + kCheckboxMargin, y_pos - 1);
  [details setFrame:details_rect];
  [checkbox_container_ addSubview:details];
}

void MediaGalleriesScanResultDialogCocoa::UpdateResults() {
  InitDialogControls();
}

void MediaGalleriesScanResultDialogCocoa::OnConstrainedWindowClosed(
    ConstrainedWindowMac* window) {
  controller_->DialogFinished(accepted_);
}

ui::MenuModel* MediaGalleriesScanResultDialogCocoa::GetContextMenu(
    MediaGalleryPrefId prefid) {
  return controller_->GetContextMenu(prefid);
}

// static
MediaGalleriesScanResultDialog* MediaGalleriesScanResultDialog::Create(
    MediaGalleriesScanResultDialogController* controller) {
  base::scoped_nsobject<MediaGalleriesScanResultCocoaController>
      cocoa_controller([[MediaGalleriesScanResultCocoaController alloc] init]);
  return new MediaGalleriesScanResultDialogCocoa(controller, cocoa_controller);
}
