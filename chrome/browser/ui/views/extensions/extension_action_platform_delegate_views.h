// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_ACTION_PLATFORM_DELEGATE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_ACTION_PLATFORM_DELEGATE_VIEWS_H_

#include "base/callback.h"
#include "chrome/browser/ui/extensions/extension_action_platform_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/views/context_menu_controller.h"

class ToolbarActionViewDelegateViews;

namespace views {
class MenuRunner;
class View;
}

// An abstract "View" for an ExtensionAction (either a BrowserAction or a
// PageAction). This contains the logic for showing the action's popup and
// the context menu. This class doesn't subclass View directly, as the
// implementations for page actions/browser actions are different types of
// views.
// All common logic for executing extension actions should go in this class;
// ToolbarActionViewDelegate classes should only have knowledge relating to
// the views::View wrapper.
class ExtensionActionPlatformDelegateViews
    : public ExtensionActionPlatformDelegate,
      public content::NotificationObserver,
      public ui::AcceleratorTarget,
      public views::ContextMenuController {
 public:
  ExtensionActionPlatformDelegateViews(
      ExtensionActionViewController* controller);
  ~ExtensionActionPlatformDelegateViews() override;

 private:
  // ExtensionActionPlatformDelegate:
  bool IsMenuRunning() const override;
  void RegisterCommand() override;
  void OnDelegateSet() override;
  void CloseActivePopup() override;
  extensions::ExtensionViewHost* ShowPopupWithUrl(
      ExtensionActionViewController::PopupShowAction show_action,
      const GURL& popup_url,
      bool grant_tab_permissions) override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  // views::ContextMenuController:
  void ShowContextMenuForView(views::View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type) override;

  // Shows the context menu for extension action.
  void DoShowContextMenu(ui::MenuSourceType source_type);

  // Unregisters the accelerator for the extension action's command, if one
  // exists. If |only_if_removed| is true, then this will only unregister if the
  // command has been removed.
  void UnregisterCommand(bool only_if_removed);

  // Closes the currently-active menu, if needed. This is the case when there
  // is an active menu that wouldn't close automatically when a new one is
  // opened.
  // Returns true if a menu was closed, false otherwise.
  bool CloseActiveMenuIfNeeded();

  ToolbarActionViewDelegateViews* GetDelegateViews() const;

  // The owning ExtensionActionViewController.
  ExtensionActionViewController* controller_;

  // Responsible for running the menu.
  scoped_ptr<views::MenuRunner> menu_runner_;

  // The extension key binding accelerator this extension action is listening
  // for (to show the popup).
  scoped_ptr<ui::Accelerator> action_keybinding_;

  // If non-NULL, this is the next ExtensionActionViewController context menu
  // which wants to run once the current owner (this one) is done.
  base::Closure followup_context_menu_task_;

  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<ExtensionActionPlatformDelegateViews> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionActionPlatformDelegateViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_ACTION_PLATFORM_DELEGATE_VIEWS_H_
