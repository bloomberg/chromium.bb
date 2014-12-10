// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/extension_action_platform_delegate_cocoa.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/extensions/extension_action_context_menu_controller.h"
#import "chrome/browser/ui/cocoa/extensions/extension_popup_controller.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_action_view_delegate_cocoa.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#import "chrome/browser/ui/cocoa/wrench_menu/wrench_menu_controller.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"

namespace {

// Returns the notification to listen to for activation for a particular
// |extension_action|.
int GetNotificationTypeForAction(const ExtensionAction& extension_action) {
  if (extension_action.action_type() == extensions::ActionInfo::TYPE_BROWSER)
    return extensions::NOTIFICATION_EXTENSION_COMMAND_BROWSER_ACTION_MAC;

  // We should only have page and browser action types.
  DCHECK_EQ(extensions::ActionInfo::TYPE_PAGE, extension_action.action_type());
  return extensions::NOTIFICATION_EXTENSION_COMMAND_PAGE_ACTION_MAC;
}

}  // namespace

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
  if (controller_->extension()->ShowConfigureContextMenus()) {
    menuController_.reset([[ExtensionActionContextMenuController alloc]
        initWithExtension:controller_->extension()
                  browser:controller_->browser()
          extensionAction:controller_->extension_action()]);
    GetDelegateCocoa()->SetContextMenuController(menuController_.get());
  }

  registrar_.Add(
      this,
      extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
      content::Source<Profile>(controller_->browser()->profile()));
  registrar_.Add(
      this,
      GetNotificationTypeForAction(*controller_->extension_action()),
      content::Source<Profile>(controller_->browser()->profile()));
}

bool ExtensionActionPlatformDelegateCocoa::IsShowingPopup() const {
  return GetPopup() != nil;
}

void ExtensionActionPlatformDelegateCocoa::CloseActivePopup() {
  ExtensionPopupController* popup = [ExtensionPopupController popup];
  if (popup && ![popup isClosing])
    [popup close];
}

void ExtensionActionPlatformDelegateCocoa::CloseOwnPopup() {
  ExtensionPopupController* popup = GetPopup();
  DCHECK(popup);
  if (popup && ![popup isClosing])
    [popup close];
}

bool ExtensionActionPlatformDelegateCocoa::ShowPopupWithUrl(
    ExtensionActionViewController::PopupShowAction show_action,
    const GURL& popup_url,
    bool grant_tab_permissions) {
  NSPoint arrowPoint = GetDelegateCocoa()->GetPopupPoint();

  // If this was triggered by an action overflowed to the wrench menu, then
  // the wrench menu will be open. Close it before opening the popup.
  WrenchMenuController* wrenchMenuController =
      [[[BrowserWindowController
          browserWindowControllerForWindow:
              controller_->browser()->window()->GetNativeWindow()]
          toolbarController] wrenchMenuController];
  if ([wrenchMenuController isMenuOpen])
    [wrenchMenuController cancel];

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

void ExtensionActionPlatformDelegateCocoa::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE:
      CloseActivePopup();
      break;
    case extensions::NOTIFICATION_EXTENSION_COMMAND_BROWSER_ACTION_MAC:
    case extensions::NOTIFICATION_EXTENSION_COMMAND_PAGE_ACTION_MAC: {
      DCHECK_EQ(type,
                GetNotificationTypeForAction(*controller_->extension_action()));
      std::pair<const std::string, gfx::NativeWindow>* payload =
          content::Details<std::pair<const std::string, gfx::NativeWindow> >(
              details).ptr();
      const std::string& extension_id = payload->first;
      gfx::NativeWindow window = payload->second;
      if (window == controller_->browser()->window()->GetNativeWindow() &&
          extension_id == controller_->extension()->id() &&
          controller_->IsEnabled(
              controller_->view_delegate()->GetCurrentWebContents())) {
        controller_->ExecuteAction(true);
      }
      break;
    }
    default:
      NOTREACHED() << L"Unexpected notification";
  }
}
