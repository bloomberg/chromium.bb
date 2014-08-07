// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_ACTION_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_ACTION_VIEW_CONTROLLER_H_

#include "chrome/browser/extensions/extension_action_icon_factory.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/gfx/image/image.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/widget/widget_observer.h"

class Browser;
class ExtensionAction;
class ExtensionActionViewDelegate;

namespace content {
class WebContents;
}

namespace extensions {
class Command;
class Extension;
}

namespace ui {
class Accelerator;
}

namespace views {
class MenuRunner;
class View;
class Widget;
}

// An abstract "View" for an ExtensionAction (either a BrowserAction or a
// PageAction). This contains the logic for showing the action's popup and
// the context menu. This class doesn't subclass View directly, as the
// implementations for page actions/browser actions are different types of
// views.
// All common logic for executing extension actions should go in this class;
// ExtensionActionViewDelegate classes should only have knowledge relating to
// the views::View wrapper.
class ExtensionActionViewController
    : public ExtensionActionIconFactory::Observer,
      public ExtensionContextMenuModel::PopupDelegate,
      public ui::AcceleratorTarget,
      public views::ContextMenuController,
      public views::WidgetObserver {
 public:
  ExtensionActionViewController(const extensions::Extension* extension,
                                Browser* browser,
                                ExtensionAction* extension_action,
                                ExtensionActionViewDelegate* delegate);
  virtual ~ExtensionActionViewController();

  // ExtensionContextMenuModel::PopupDelegate:
  virtual void InspectPopup() OVERRIDE;

  // Executes the default extension action (typically showing the popup), and
  // attributes the action to a user (thus, only use this for actions that
  // *were* done by the user).
  void ExecuteActionByUser();

  // Executes the extension action with |show_action|. If
  // |grant_tab_permissions| is true, this will grant the extension active tab
  // permissions. Only do this if this was done through a user action (and not
  // e.g. an API). Returns true if a popup is shown.
  bool ExecuteAction(ExtensionPopup::ShowAction show_action,
                     bool grant_tab_permissions);

  // Hides the popup, if one is open.
  void HidePopup();

  // Returns the icon from the |icon_factory_|.
  gfx::Image GetIcon(int tab_id);

  // Returns the current tab id.
  int GetCurrentTabId() const;

  // Registers an accelerator for the extension action's command, if one
  // exists.
  void RegisterCommand();

  // Unregisters the accelerator for the extension action's command, if one
  // exists. If |only_if_removed| is true, then this will only unregister if the
  // command has been removed.
  void UnregisterCommand(bool only_if_removed);

  const extensions::Extension* extension() const { return extension_; }
  Browser* browser() { return browser_; }
  ExtensionAction* extension_action() { return extension_action_; }
  const ExtensionAction* extension_action() const { return extension_action_; }
  ExtensionPopup* popup() { return popup_; }
  bool is_menu_running() const { return menu_runner_.get() != NULL; }

 private:
  // ExtensionActionIconFactory::Observer:
  virtual void OnIconUpdated() OVERRIDE;

  // ui::AcceleratorTarget:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;
  virtual bool CanHandleAccelerators() const OVERRIDE;

  // views::WidgetObserver:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

  // views::ContextMenuController:
  virtual void ShowContextMenuForView(views::View* source,
                                      const gfx::Point& point,
                                      ui::MenuSourceType source_type) OVERRIDE;

  // Shows the popup for the extension action, given the associated |popup_url|.
  // Returns true if a popup is successfully shown.
  bool ShowPopupWithUrl(ExtensionPopup::ShowAction show_action,
                        const GURL& popup_url);

  // Populates |command| with the command associated with |extension|, if one
  // exists. Returns true if |command| was populated.
  bool GetExtensionCommand(extensions::Command* command);

  // Cleans up after the popup. If |close_widget| is true, this will call
  // Widget::Close() on the popup's widget; otherwise it assumes the popup is
  // already closing.
  void CleanupPopup(bool close_widget);

  // The extension associated with the action we're displaying.
  const extensions::Extension* extension_;

  // The corresponding browser.
  Browser* browser_;

  // The browser action this view represents. The ExtensionAction is not owned
  // by this class.
  ExtensionAction* extension_action_;

  // Our delegate.
  ExtensionActionViewDelegate* delegate_;

  // The object that will be used to get the browser action icon for us.
  // It may load the icon asynchronously (in which case the initial icon
  // returned by the factory will be transparent), so we have to observe it for
  // updates to the icon.
  ExtensionActionIconFactory icon_factory_;

  // Responsible for running the menu.
  scoped_ptr<views::MenuRunner> menu_runner_;

  // The browser action's popup, if it is visible; NULL otherwise.
  ExtensionPopup* popup_;

  // The extension key binding accelerator this extension action is listening
  // for (to show the popup).
  scoped_ptr<ui::Accelerator> action_keybinding_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionActionViewController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_ACTION_VIEW_CONTROLLER_H_
