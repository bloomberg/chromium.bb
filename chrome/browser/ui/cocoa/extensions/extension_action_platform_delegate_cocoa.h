// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_ACTION_PLATFORM_DELEGATE_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_ACTION_PLATFORM_DELEGATE_COCOA_H_

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/extensions/extension_action_platform_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

@class ExtensionActionContextMenuController;
@class ExtensionPopupController;
class ToolbarActionViewDelegateCocoa;

// The cocoa-specific implementation for ExtensionActionPlatformDelegate.
class ExtensionActionPlatformDelegateCocoa
    : public ExtensionActionPlatformDelegate,
      public content::NotificationObserver {
 public:
  ExtensionActionPlatformDelegateCocoa(
      ExtensionActionViewController* controller);
  ~ExtensionActionPlatformDelegateCocoa() override;

 private:
  // ExtensionActionPlatformDelegate:
  gfx::NativeView GetPopupNativeView() override;
  bool IsMenuRunning() const override;
  void RegisterCommand() override;
  void OnDelegateSet() override;
  bool IsShowingPopup() const override;
  void CloseActivePopup() override;
  void CloseOwnPopup() override;
  bool ShowPopupWithUrl(
      ExtensionActionViewController::PopupShowAction show_action,
      const GURL& popup_url,
      bool grant_tab_permissions) override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Returns the popup shown by this extension action, if one exists.
  ExtensionPopupController* GetPopup() const;

  // Returns the delegate in its cocoa implementation.
  ToolbarActionViewDelegateCocoa* GetDelegateCocoa();

  // The main controller for this extension action.
  ExtensionActionViewController* controller_;

  // The context menu controller for the extension action, if any.
  base::scoped_nsobject<ExtensionActionContextMenuController> menuController_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionActionPlatformDelegateCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_ACTION_PLATFORM_DELEGATE_COCOA_H_
