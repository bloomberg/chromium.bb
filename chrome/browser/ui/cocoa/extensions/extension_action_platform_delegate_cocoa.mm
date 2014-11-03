// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/extension_action_platform_delegate_cocoa.h"

#import "chrome/browser/ui/cocoa/extensions/extension_popup_controller.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_action_view_delegate_cocoa.h"
#include "extensions/common/extension.h"

// static
scoped_ptr<ExtensionActionPlatformDelegate>
ExtensionActionPlatformDelegate::Create(
    ExtensionActionViewController* controller) {
  return make_scoped_ptr(new ExtensionActionPlatformDelegateCocoa(controller));
}

ExtensionActionPlatformDelegateCocoa::ExtensionActionPlatformDelegateCocoa(
    ExtensionActionViewController* controller)
    : controller_(controller) {
}

ExtensionActionPlatformDelegateCocoa::~ExtensionActionPlatformDelegateCocoa() {
}

gfx::NativeView ExtensionActionPlatformDelegateCocoa::GetPopupNativeView() {
  ExtensionPopupController* popup = GetPopup();
  return popup != nil ? [popup view] : nil;
}

bool ExtensionActionPlatformDelegateCocoa::IsMenuRunning() const {
  // TODO(devlin): Also account for context menus.
  return GetPopup() != nil;
}

void ExtensionActionPlatformDelegateCocoa::RegisterCommand() {
  // Commands are handled elsewhere for cocoa.
}

void ExtensionActionPlatformDelegateCocoa::OnDelegateSet() {
}

bool ExtensionActionPlatformDelegateCocoa::IsShowingPopup() const {
  return GetPopup() != nil;
}

void ExtensionActionPlatformDelegateCocoa::CloseActivePopup() {
  ExtensionPopupController* popup = [ExtensionPopupController popup];
  if (popup)
    [popup close];
}

void ExtensionActionPlatformDelegateCocoa::CloseOwnPopup() {
  ExtensionPopupController* popup = GetPopup();
  DCHECK(popup);
  [popup close];
}

bool ExtensionActionPlatformDelegateCocoa::ShowPopupWithUrl(
    ExtensionActionViewController::PopupShowAction show_action,
    const GURL& popup_url,
    bool grant_tab_permissions) {
  NSPoint arrowPoint = GetDelegateCocoa()->GetPopupPoint();
  [ExtensionPopupController showURL:popup_url
                          inBrowser:controller_->browser()
                         anchoredAt:arrowPoint
                      arrowLocation:info_bubble::kTopRight
                            devMode:NO];
  return true;
}

ToolbarActionViewDelegateCocoa*
ExtensionActionPlatformDelegateCocoa::GetDelegateCocoa() {
  return static_cast<ToolbarActionViewDelegateCocoa*>(
      controller_->view_delegate());
}

ExtensionPopupController* ExtensionActionPlatformDelegateCocoa::GetPopup()
    const {
  ExtensionPopupController* popup = [ExtensionPopupController popup];
  return popup && [popup extensionId] == controller_->extension()->id() ?
      popup : nil;
}
