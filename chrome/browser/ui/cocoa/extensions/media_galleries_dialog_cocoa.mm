// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/extensions/media_galleries_dialog_cocoa.h"

#include "base/sys_string_conversions.h"
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

- (void)onAddFolderClicked:(id)sender {
  DCHECK(dialog_);
  dialog_->OnAddFolderClicked();
}

- (void)onCheckboxToggled:(id)sender {
  DCHECK(dialog_);
  dialog_->OnCheckboxToggled(sender);
}

- (void)sheetDidEnd:(NSWindow*)parent
         returnCode:(NSInteger)returnCode
            context:(void*)context {
  DCHECK(dialog_);
  dialog_->SheetDidEnd(returnCode);
}

@end

namespace chrome {

MediaGalleriesDialogCocoa::MediaGalleriesDialogCocoa(
    MediaGalleriesDialogController* controller,
    MediaGalleriesCocoaController* cocoa_controller)
    : ConstrainedWindowMacDelegateSystemSheet(
          cocoa_controller, @selector(sheetDidEnd:returnCode:context:)),
      controller_(controller),
      window_(NULL),
      accepted_(false),
      cocoa_controller_([cocoa_controller retain]) {
  [cocoa_controller_ setDialog:this];

  alert_.reset([[NSAlert alloc] init]);
  [alert_ setMessageText:base::SysUTF16ToNSString(controller_->GetHeader())];
  [alert_ setInformativeText:SysUTF16ToNSString(controller_->GetSubtext())];
  [alert_ addButtonWithTitle:l10n_util::GetNSString(
      IDS_MEDIA_GALLERIES_DIALOG_CONFIRM)];
  [alert_ addButtonWithTitle:l10n_util::GetNSString(
      IDS_MEDIA_GALLERIES_DIALOG_CANCEL)];
  [alert_ addButtonWithTitle:l10n_util::GetNSString(
      IDS_MEDIA_GALLERIES_DIALOG_ADD_GALLERY)];

  // Override the add button click handler to prevent the alert from closing.
  NSButton* add_button = [[alert_ buttons] objectAtIndex:2];
  [add_button setTarget:cocoa_controller_];
  [add_button setAction:@selector(onAddFolderClicked:)];

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

  set_sheet(alert_);
  // May be NULL during tests.
  if (controller->tab_contents())
    window_ = new ConstrainedWindowMac(controller->tab_contents(), this);
}

MediaGalleriesDialogCocoa::~MediaGalleriesDialogCocoa() {
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
    NSString* device_id = base::SysUTF8ToNSString(gallery->device_id);
    if ([[[checkbox cell] representedObject] isEqual:device_id]) {
      controller_->DidToggleGallery(gallery, [checkbox state] == NSOnState);
      break;
    }
  }
}

void MediaGalleriesDialogCocoa::SheetDidEnd(NSInteger result) {
  switch (result) {
    case NSAlertFirstButtonReturn:
      accepted_ = true;
      if (window_)
        window_->CloseConstrainedWindow();
      break;
    case NSAlertSecondButtonReturn:
      if (window_)
        window_->CloseConstrainedWindow();
      break;
    default:
      NOTREACHED();
      break;
  }
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
    NSString* device_id = base::SysUTF8ToNSString(gallery->device_id);
    [[new_checkbox cell] setRepresentedObject:device_id];
    [[new_checkbox cell] setLineBreakMode:NSLineBreakByTruncatingMiddle];
    [new_checkbox setButtonType:NSSwitchButton];
    [new_checkbox setTarget:cocoa_controller_];
    [new_checkbox setAction:@selector(onCheckboxToggled:)];

    [checkbox_container_ addSubview:new_checkbox];
    [checkboxes_ addObject:new_checkbox];
    checkbox = new_checkbox.get();
  }

  [checkbox setTitle:base::SysUTF16ToNSString(gallery->display_name)];
  [checkbox setToolTip:
      base::SysUTF16ToNSString(gallery->path.LossyDisplayName())];
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
  NSButton* checkbox = nil;
  NSString* device_id = base::SysUTF8ToNSString(gallery->device_id);

  for (NSButton* button in checkboxes_.get()) {
    if ([[[button cell] representedObject] isEqual:device_id]) {
      checkbox = button;
      break;
    }
  }

  UpdateGalleryCheckbox(checkbox, gallery, permitted);
  UpdateCheckboxContainerFrame();
  [alert_ layout];
}

void MediaGalleriesDialogCocoa::DeleteDelegate() {
  // As required by ConstrainedWindowMacDelegate, close the sheet if
  // it's still open.
  if (is_sheet_open())
    [NSApp endSheet:sheet()];

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
