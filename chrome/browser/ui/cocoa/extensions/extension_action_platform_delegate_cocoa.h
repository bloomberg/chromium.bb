// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_ACTION_PLATFORM_DELEGATE_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_ACTION_PLATFORM_DELEGATE_COCOA_H_

#include "chrome/browser/ui/extensions/extension_action_platform_delegate.h"

@class ExtensionPopupController;
class ToolbarActionViewDelegateCocoa;

// The cocoa-specific implementation for ExtensionActionPlatformDelegate.
class ExtensionActionPlatformDelegateCocoa
    : public ExtensionActionPlatformDelegate {
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

  // Returns the popup shown by this extension action, if one exists.
  ExtensionPopupController* GetPopup() const;

  // Returns the delegate in its cocoa implementation.
  ToolbarActionViewDelegateCocoa* GetDelegateCocoa();

  ExtensionActionViewController* controller_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionActionPlatformDelegateCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_ACTION_PLATFORM_DELEGATE_COCOA_H_
