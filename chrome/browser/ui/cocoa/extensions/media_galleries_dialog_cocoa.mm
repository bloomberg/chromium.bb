// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/extensions/media_galleries_dialog_cocoa.h"

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_alert.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_button.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_control_utils.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"
#import "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#import "ui/base/cocoa/flipped_view.h"
#import "ui/base/cocoa/menu_controller.h"
#import "ui/base/models/menu_model.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

// Controller for UI events on items in the media galleries dialog.
@interface MediaGalleriesCocoaController : NSObject {
 @private
  MediaGalleriesDialogCocoa* dialog_;
}

@property(assign, nonatomic) MediaGalleriesDialogCocoa* dialog;

@end

@implementation MediaGalleriesCocoaController

@synthesize dialog = dialog_;

- (void)onAcceptButton:(id)sender {
  dialog_->OnAcceptClicked();
}

- (void)onCancelButton:(id)sender {
  dialog_->OnCancelClicked();
}

- (void)onAddFolderClicked:(id)sender {
  DCHECK(dialog_);
  dialog_->OnAddFolderClicked();
}

- (void)onCheckboxToggled:(id)sender {
  DCHECK(dialog_);
  dialog_->OnCheckboxToggled(sender);
}

@end


@interface MediaGalleriesCheckbox : NSButton {
 @private
  MediaGalleriesDialogCocoa* dialog_;  // |dialog_| owns |this|.
  MediaGalleryPrefId prefId_;
  base::scoped_nsobject<MenuController> menuController_;
}

- (id)initWithFrame:(NSRect)frameRect
             dialog:(MediaGalleriesDialogCocoa*)dialog
             prefId:(MediaGalleryPrefId)prefId;
- (NSMenu*)menuForEvent:(NSEvent*)theEvent;

@end

@implementation MediaGalleriesCheckbox

- (id)initWithFrame:(NSRect)frameRect
             dialog:(MediaGalleriesDialogCocoa*)dialog
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

NSString* GetUniqueIDForGallery(const MediaGalleryPrefInfo& gallery) {
  return base::SysUTF8ToNSString(gallery.device_id + gallery.path.value());
}

}  // namespace

MediaGalleriesDialogCocoa::MediaGalleriesDialogCocoa(
    MediaGalleriesDialogController* controller,
    MediaGalleriesCocoaController* cocoa_controller)
    : controller_(controller),
      accepted_(false),
      cocoa_controller_([cocoa_controller retain]) {
  [cocoa_controller_ setDialog:this];

  alert_.reset([[ConstrainedWindowAlert alloc] init]);

  [alert_ setMessageText:base::SysUTF16ToNSString(controller_->GetHeader())];
  [alert_ setInformativeText:
      base::SysUTF16ToNSString(controller_->GetSubtext())];
  [alert_ addButtonWithTitle:
    l10n_util::GetNSString(IDS_MEDIA_GALLERIES_DIALOG_CONFIRM)
               keyEquivalent:kKeyEquivalentReturn
                      target:cocoa_controller_
                      action:@selector(onAcceptButton:)];
  [alert_ addButtonWithTitle:
      l10n_util::GetNSString(IDS_MEDIA_GALLERIES_DIALOG_CANCEL)
               keyEquivalent:kKeyEquivalentEscape
                      target:cocoa_controller_
                      action:@selector(onCancelButton:)];
  [alert_ addButtonWithTitle:
      l10n_util::GetNSString(IDS_MEDIA_GALLERIES_DIALOG_ADD_GALLERY)
               keyEquivalent:kKeyEquivalentNone
                      target:cocoa_controller_
                      action:@selector(onAddFolderClicked:)];
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

MediaGalleriesDialogCocoa::~MediaGalleriesDialogCocoa() {
}

void MediaGalleriesDialogCocoa::InitDialogControls() {
  main_container_.reset([[NSBox alloc] init]);
  [main_container_ setBoxType:NSBoxCustom];
  [main_container_ setBorderType:NSLineBorder];
  [main_container_ setBorderWidth:1];
  [main_container_ setCornerRadius:0];
  [main_container_ setContentViewMargins:NSZeroSize];
  [main_container_ setTitlePosition:NSNoTitle];
  [main_container_ setBorderColor:[NSColor colorWithCalibratedRed:0.625
                                                            green:0.625
                                                             blue:0.625
                                                            alpha:1.0]];

  base::scoped_nsobject<NSScrollView> scroll_view(
      [[NSScrollView alloc] initWithFrame:
          NSMakeRect(0, 0, kCheckboxMaxWidth, kScrollAreaHeight)]);
  [scroll_view setHasVerticalScroller:YES];
  [scroll_view setHasHorizontalScroller:NO];
  [scroll_view setBorderType:NSNoBorder];
  [scroll_view setAutohidesScrollers:YES];
  [[main_container_ contentView] addSubview:scroll_view];

  // Add gallery permission checkboxes inside the scrolling view.
  checkbox_container_.reset([[FlippedView alloc] initWithFrame:NSZeroRect]);
  checkboxes_.reset([[NSMutableArray alloc] init]);
  [scroll_view setDocumentView:checkbox_container_];

  CGFloat y_pos = 0;

  y_pos = CreateAttachedCheckboxes(y_pos, controller_->AttachedPermissions());

  if (!controller_->UnattachedPermissions().empty()) {
    y_pos = CreateCheckboxSeparator(y_pos);

    y_pos = CreateUnattachedCheckboxes(
        y_pos, controller_->UnattachedPermissions());
  }

  [checkbox_container_ setFrame:NSMakeRect(0, 0, kCheckboxMaxWidth, y_pos + 2)];

  // Resize to pack the scroll view if possible.
  NSRect scroll_frame = [scroll_view frame];
  if (NSHeight(scroll_frame) > NSHeight([checkbox_container_ frame])) {
    scroll_frame.size.height = NSHeight([checkbox_container_ frame]);
    [scroll_view setFrame:scroll_frame];
  }

  [main_container_ setFrameFromContentFrame:scroll_frame];
  [alert_ setAccessoryView:main_container_];

  // As a safeguard against the user skipping reading over the dialog and just
  // confirming, the button will be unavailable for dialogs without any checks
  // until the user toggles something.
  [[[alert_ buttons] objectAtIndex:0] setEnabled:
      controller_->HasPermittedGalleries()];

  [alert_ layout];
}

CGFloat MediaGalleriesDialogCocoa::CreateAttachedCheckboxes(
    CGFloat y_pos,
    const MediaGalleriesDialogController::GalleryPermissionsVector&
        permissions) {
  y_pos += kCheckboxMargin;

  for (MediaGalleriesDialogController::GalleryPermissionsVector::
       const_iterator iter = permissions.begin();
       iter != permissions.end(); iter++) {
    const MediaGalleriesDialogController::GalleryPermission& permission = *iter;
    UpdateGalleryCheckbox(permission.pref_info, permission.allowed, y_pos);
    y_pos = NSMaxY([[checkboxes_ lastObject] frame]) + kCheckboxMargin;
  }

  return y_pos;
}

// Add checkboxes for galleries that aren't available (i.e. removable
// volumes that are not currently attached).
CGFloat MediaGalleriesDialogCocoa::CreateUnattachedCheckboxes(
    CGFloat y_pos,
    const MediaGalleriesDialogController::GalleryPermissionsVector&
        permissions) {
  y_pos += kCheckboxMargin;

  for (MediaGalleriesDialogController::GalleryPermissionsVector::
       const_iterator iter = permissions.begin();
       iter != permissions.end(); iter++) {
    const MediaGalleriesDialogController::GalleryPermission& permission = *iter;
    UpdateGalleryCheckbox(permission.pref_info, permission.allowed, y_pos);
    y_pos = NSMaxY([[checkboxes_ lastObject] frame]) + kCheckboxMargin;
  }

  return y_pos;
}

CGFloat MediaGalleriesDialogCocoa::CreateCheckboxSeparator(CGFloat y_pos) {
  base::scoped_nsobject<NSBox> separator(
      [[NSBox alloc] initWithFrame:NSMakeRect(
          0, y_pos + kCheckboxMargin * 0.5, kCheckboxMaxWidth, 1.0)]);
  [separator setBoxType:NSBoxSeparator];
  [separator setBorderType:NSLineBorder];
  [separator setAlphaValue:0.2];
  [checkbox_container_ addSubview:separator];
  y_pos += kCheckboxMargin * 0.5 + 4;

  base::scoped_nsobject<NSTextField> unattached_label(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  [unattached_label setEditable:NO];
  [unattached_label setSelectable:NO];
  [unattached_label setBezeled:NO];
  [unattached_label setAttributedStringValue:
      constrained_window::GetAttributedLabelString(
          base::SysUTF16ToNSString(
              controller_->GetUnattachedLocationsHeader()),
          chrome_style::kTextFontStyle,
          NSNaturalTextAlignment,
          NSLineBreakByClipping
      )];
  [unattached_label sizeToFit];
  NSSize unattached_label_size = [unattached_label frame].size;
  [unattached_label setFrame:NSMakeRect(
      kCheckboxMargin, y_pos + kCheckboxMargin,
      kCheckboxMaxWidth, unattached_label_size.height)];
  [checkbox_container_ addSubview:unattached_label];
  y_pos = NSMaxY([unattached_label frame]) + kCheckboxMargin;

  return y_pos;
}

void MediaGalleriesDialogCocoa::OnAcceptClicked() {
  accepted_ = true;

  if (window_)
    window_->CloseWebContentsModalDialog();
}

void MediaGalleriesDialogCocoa::OnCancelClicked() {
  if (window_)
    window_->CloseWebContentsModalDialog();
}

void MediaGalleriesDialogCocoa::OnAddFolderClicked() {
  controller_->OnAddFolderClicked();
}

void MediaGalleriesDialogCocoa::OnCheckboxToggled(NSButton* checkbox) {
  [[[alert_ buttons] objectAtIndex:0] setEnabled:YES];

  const MediaGalleriesDialogController::GalleryPermissionsVector&
      attached_permissions = controller_->AttachedPermissions();
  for (MediaGalleriesDialogController::GalleryPermissionsVector::
       const_reverse_iterator iter = attached_permissions.rbegin();
       iter != attached_permissions.rend(); iter++) {
    const MediaGalleryPrefInfo* gallery = &iter->pref_info;
    NSString* unique_id = GetUniqueIDForGallery(*gallery);
    if ([[[checkbox cell] representedObject] isEqual:unique_id]) {
      controller_->DidToggleGalleryId(gallery->pref_id,
                                      [checkbox state] == NSOnState);
      break;
    }
  }

  const MediaGalleriesDialogController::GalleryPermissionsVector&
      unattached_permissions = controller_->UnattachedPermissions();
  for (MediaGalleriesDialogController::GalleryPermissionsVector::
       const_reverse_iterator iter = unattached_permissions.rbegin();
       iter != unattached_permissions.rend(); iter++) {
    const MediaGalleryPrefInfo* gallery = &iter->pref_info;
    NSString* unique_id = GetUniqueIDForGallery(*gallery);
    if ([[[checkbox cell] representedObject] isEqual:unique_id]) {
      controller_->DidToggleGalleryId(gallery->pref_id,
                                      [checkbox state] == NSOnState);
      break;
    }
  }

}

void MediaGalleriesDialogCocoa::UpdateGalleryCheckbox(
    const MediaGalleryPrefInfo& gallery,
    bool permitted,
    CGFloat y_pos) {
  // Checkbox.
  base::scoped_nsobject<MediaGalleriesCheckbox> checkbox(
      [[MediaGalleriesCheckbox alloc] initWithFrame:NSZeroRect
                                             dialog:this
                                             prefId:gallery.pref_id]);
  NSString* unique_id = GetUniqueIDForGallery(gallery);
  [[checkbox cell] setRepresentedObject:unique_id];
  [[checkbox cell] setLineBreakMode:NSLineBreakByTruncatingMiddle];
  [checkbox setButtonType:NSSwitchButton];
  [checkbox setTarget:cocoa_controller_];
  [checkbox setAction:@selector(onCheckboxToggled:)];
  [checkboxes_ addObject:checkbox];

  [checkbox setTitle:base::SysUTF16ToNSString(
      gallery.GetGalleryDisplayName())];
  [checkbox setToolTip:base::SysUTF16ToNSString(gallery.GetGalleryTooltip())];
  [checkbox setState:permitted ? NSOnState : NSOffState];

  [checkbox sizeToFit];
  NSRect rect = [checkbox bounds];

  // Detail text.
  base::scoped_nsobject<NSTextField> details(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
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
  [details setTextColor:[NSColor colorWithCalibratedRed:0.625
                                                  green:0.625
                                                   blue:0.625
                                                  alpha:1.0]];
  [details sizeToFit];
  NSRect details_rect = [details bounds];

  // The checkbox will elide so reduce its size so it will all fit.
  rect.size.width =
      std::min(NSWidth(rect),
               kCheckboxMaxWidth - 3 * kCheckboxMargin - NSWidth(details_rect));
  rect.origin = NSMakePoint(kCheckboxMargin, y_pos);
  [checkbox setFrame:rect];
  [checkbox_container_ addSubview:checkbox];

  details_rect.origin = NSMakePoint(NSMaxX(rect) + kCheckboxMargin, y_pos - 1);
  [details setFrame:details_rect];

  [checkbox_container_ addSubview:details];
}

void MediaGalleriesDialogCocoa::UpdateGalleries() {
  InitDialogControls();
}

void MediaGalleriesDialogCocoa::OnConstrainedWindowClosed(
    ConstrainedWindowMac* window) {
  controller_->DialogFinished(accepted_);
}

ui::MenuModel* MediaGalleriesDialogCocoa::GetContextMenu(
    MediaGalleryPrefId prefid) {
  return controller_->GetContextMenu(prefid);
}

// static
MediaGalleriesDialog* MediaGalleriesDialog::Create(
    MediaGalleriesDialogController* controller) {
  base::scoped_nsobject<MediaGalleriesCocoaController> cocoa_controller(
      [[MediaGalleriesCocoaController alloc] init]);
  return new MediaGalleriesDialogCocoa(controller, cocoa_controller);
}
