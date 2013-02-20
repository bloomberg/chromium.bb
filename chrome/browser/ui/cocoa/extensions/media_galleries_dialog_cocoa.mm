// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/extensions/media_galleries_dialog_cocoa.h"

#include "base/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_alert.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"
#import "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

const CGFloat kCheckboxMargin = 5;
const CGFloat kCheckboxMaxWidth = 350;

@interface MediaGalleriesCocoaController : NSObject {
 @private
  chrome::MediaGalleriesDialogCocoa* dialog_;
}

@property(nonatomic, assign) chrome::MediaGalleriesDialogCocoa* dialog;

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

namespace chrome {

namespace {

NSString* GetUniqueIDForGallery(const MediaGalleryPrefInfo* gallery) {
  return base::SysUTF8ToNSString(gallery->device_id + gallery->path.value());
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
  [alert_ setInformativeText:SysUTF16ToNSString(controller_->GetSubtext())];
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

  // Add gallery permission checkboxes inside an accessory view.
  checkbox_container_.reset([[NSView alloc] initWithFrame:NSZeroRect]);
  checkboxes_.reset([[NSMutableArray alloc] init]);
  const MediaGalleriesDialogController::KnownGalleryPermissions& permissions =
      controller_->permissions();
  for (MediaGalleriesDialogController::KnownGalleryPermissions::
       const_reverse_iterator iter = permissions.rbegin();
       iter != permissions.rend(); iter++) {
    const MediaGalleriesDialogController::GalleryPermission& permission =
        iter->second;
    UpdateGalleryCheckbox(nil, &permission.pref_info, permission.allowed);
  }
  UpdateCheckboxContainerFrame();
  [alert_ setAccessoryView:checkbox_container_];

  // As a safeguard against the user skipping reading over the dialog and just
  // confirming, the button will be unavailable for dialogs without any checks
  // until the user toggles something.
  [[[alert_ buttons] objectAtIndex:0] setEnabled:
      controller_->HasPermittedGalleries()];

  [alert_ layout];

  // May be NULL during tests.
  if (controller->web_contents()) {
    scoped_nsobject<CustomConstrainedWindowSheet> sheet(
        [[CustomConstrainedWindowSheet alloc]
            initWithCustomWindow:[alert_ window]]);
    window_.reset(new ConstrainedWindowMac(
        this, controller->web_contents(), sheet));
  }
}

MediaGalleriesDialogCocoa::~MediaGalleriesDialogCocoa() {
}

void MediaGalleriesDialogCocoa::OnAcceptClicked() {
  accepted_ = true;
  window_->CloseWebContentsModalDialog();
}

void MediaGalleriesDialogCocoa::OnCancelClicked() {
  window_->CloseWebContentsModalDialog();
}

void MediaGalleriesDialogCocoa::OnAddFolderClicked() {
  controller_->OnAddFolderClicked();
}

void MediaGalleriesDialogCocoa::OnCheckboxToggled(NSButton* checkbox) {
 const MediaGalleriesDialogController::KnownGalleryPermissions& permissions =
      controller_->permissions();
  [[[alert_ buttons] objectAtIndex:0] setEnabled:YES];

  for (MediaGalleriesDialogController::KnownGalleryPermissions::
       const_reverse_iterator iter = permissions.rbegin();
       iter != permissions.rend(); iter++) {
    const MediaGalleryPrefInfo* gallery = &iter->second.pref_info;
    NSString* unique_id = GetUniqueIDForGallery(gallery);
    if ([[[checkbox cell] representedObject] isEqual:unique_id]) {
      controller_->DidToggleGallery(gallery, [checkbox state] == NSOnState);
      break;
    }
  }
}

NSButton* MediaGalleriesDialogCocoa::CheckboxForGallery(
    const MediaGalleryPrefInfo* gallery) {
  NSString* unique_id = GetUniqueIDForGallery(gallery);
  for (NSButton* button in checkboxes_.get()) {
    if ([[[button cell] representedObject] isEqual:unique_id])
      return button;
  }
  return nil;
}

void MediaGalleriesDialogCocoa::UpdateGalleryCheckbox(
    NSButton* checkbox,
    const MediaGalleryPrefInfo* gallery,
    bool permitted) {
  CGFloat y_pos = 0;
  if (checkbox) {
    y_pos = NSMinY([checkbox frame]);
  } else {
    if ([checkboxes_ count] > 0)
      y_pos = NSMaxY([[checkboxes_ lastObject] frame]) + kCheckboxMargin;

    scoped_nsobject<NSButton> new_checkbox(
        [[NSButton alloc] initWithFrame:NSZeroRect]);
    NSString* unique_id = GetUniqueIDForGallery(gallery);
    [[new_checkbox cell] setRepresentedObject:unique_id];
    [[new_checkbox cell] setLineBreakMode:NSLineBreakByTruncatingMiddle];
    [new_checkbox setButtonType:NSSwitchButton];
    [new_checkbox setTarget:cocoa_controller_];
    [new_checkbox setAction:@selector(onCheckboxToggled:)];

    [checkbox_container_ addSubview:new_checkbox];
    [checkboxes_ addObject:new_checkbox];
    checkbox = new_checkbox.get();
  }

  [checkbox setTitle:base::SysUTF16ToNSString(
      MediaGalleriesDialogController::GetGalleryDisplayName(*gallery))];
  [checkbox setToolTip:base::SysUTF16ToNSString(
      MediaGalleriesDialogController::GetGalleryTooltip(*gallery))];
  [checkbox setState:permitted ? NSOnState : NSOffState];

  [checkbox sizeToFit];
  NSRect rect = [checkbox bounds];
  rect.origin.y = y_pos;
  rect.size.width = std::min(NSWidth(rect), kCheckboxMaxWidth);
  [checkbox setFrame:rect];
}

void MediaGalleriesDialogCocoa::UpdateCheckboxContainerFrame() {
  NSRect rect = NSMakeRect(
      0, 0, kCheckboxMaxWidth, NSMaxY([[checkboxes_ lastObject] frame]));
  [checkbox_container_ setFrame:rect];
}

void MediaGalleriesDialogCocoa::UpdateGallery(
    const MediaGalleryPrefInfo* gallery,
    bool permitted) {
  NSButton* checkbox = CheckboxForGallery(gallery);
  UpdateGalleryCheckbox(checkbox, gallery, permitted);
  UpdateCheckboxContainerFrame();
  [alert_ layout];
}

void MediaGalleriesDialogCocoa::ForgetGallery(
    const MediaGalleryPrefInfo* gallery) {
  NSButton* checkbox = CheckboxForGallery(gallery);
  if (!checkbox)
    return;

  // Remove checkbox and reposition the entries below it.
  NSUInteger i = [checkboxes_ indexOfObject:checkbox];
  [checkboxes_ removeObjectAtIndex:i];
  for (; i < [checkboxes_ count]; ++i) {
    CGFloat y_pos = 0;
    if (i > 0) {
      y_pos = NSMaxY([[checkboxes_ objectAtIndex:i - 1] frame]) +
          kCheckboxMargin;
    }
    checkbox = [checkboxes_ objectAtIndex:i];
    NSRect rect = [checkbox bounds];
    rect.origin.y = y_pos;
    [checkbox setFrame:rect];
  }
  UpdateCheckboxContainerFrame();
  [alert_ layout];
}

void MediaGalleriesDialogCocoa::OnConstrainedWindowClosed(
    ConstrainedWindowMac* window) {
  controller_->DialogFinished(accepted_);
}

// static
MediaGalleriesDialog* MediaGalleriesDialog::Create(
    MediaGalleriesDialogController* controller) {
  scoped_nsobject<MediaGalleriesCocoaController> cocoa_controller(
      [[MediaGalleriesCocoaController alloc] init]);
  return new MediaGalleriesDialogCocoa(controller, cocoa_controller);
}

}  // namespace chrome
