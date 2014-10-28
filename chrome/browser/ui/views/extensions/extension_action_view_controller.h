// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_ACTION_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_ACTION_VIEW_CONTROLLER_H_

#include "chrome/browser/extensions/extension_action_icon_factory.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/gfx/image/image.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/widget/widget_observer.h"

class Browser;
class ExtensionAction;
class ToolbarActionViewDelegate;

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
// ToolbarActionViewDelegate classes should only have knowledge relating to
// the views::View wrapper.
class ExtensionActionViewController
    : public ToolbarActionViewController,
      public content::NotificationObserver,
      public ExtensionActionIconFactory::Observer,
      public ExtensionContextMenuModel::PopupDelegate,
      public ui::AcceleratorTarget,
      public views::ContextMenuController,
      public views::WidgetObserver {
 public:
  ExtensionActionViewController(const extensions::Extension* extension,
                                Browser* browser,
                                ExtensionAction* extension_action);
  ~ExtensionActionViewController() override;

  // ToolbarActionViewController:
  const std::string& GetId() const override;
  void SetDelegate(ToolbarActionViewDelegate* delegate) override;
  gfx::Image GetIcon(content::WebContents* web_contents) override;
  gfx::ImageSkia GetIconWithBadge() override;
  base::string16 GetAccessibleName(
      content::WebContents* web_contents) const override;
  base::string16 GetTooltip(content::WebContents* web_contents) const override;
  bool IsEnabled(content::WebContents* web_contents) const override;
  bool HasPopup(content::WebContents* web_contents) const override;
  void HidePopup() override;
  gfx::NativeView GetPopupNativeView() override;
  bool IsMenuRunning() const override;
  bool CanDrag() const override;
  bool ExecuteAction(bool by_user) override;
  void PaintExtra(gfx::Canvas* canvas,
                  const gfx::Rect& bounds,
                  content::WebContents* web_contents) const override;
  void RegisterCommand() override;

  // ExtensionContextMenuModel::PopupDelegate:
  void InspectPopup() override;

  // Executes the extension action with |show_action|. If
  // |grant_tab_permissions| is true, this will grant the extension active tab
  // permissions. Only do this if this was done through a user action (and not
  // e.g. an API). Returns true if a popup is shown.
  bool ExecuteAction(ExtensionPopup::ShowAction show_action,
                     bool grant_tab_permissions);

  void set_icon_observer(ExtensionActionIconFactory::Observer* icon_observer) {
    icon_observer_ = icon_observer;
  }

  const extensions::Extension* extension() const { return extension_; }
  Browser* browser() { return browser_; }
  ExtensionAction* extension_action() { return extension_action_; }
  const ExtensionAction* extension_action() const { return extension_action_; }
  ExtensionPopup* popup() { return popup_; }

 private:
  // ExtensionActionIconFactory::Observer:
  void OnIconUpdated() override;

  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // views::ContextMenuController:
  void ShowContextMenuForView(views::View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type) override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Shows the context menu for extension action.
  void DoShowContextMenu(ui::MenuSourceType source_type);

  // Shows the popup for the extension action, given the associated |popup_url|.
  // |grant_tab_permissions| is true if active tab permissions should be given
  // to the extension; this is only true if the popup is opened through a user
  // action.
  // Returns true if a popup is successfully shown.
  bool ShowPopupWithUrl(ExtensionPopup::ShowAction show_action,
                        const GURL& popup_url,
                        bool grant_tab_permissions);

  // Populates |command| with the command associated with |extension|, if one
  // exists. Returns true if |command| was populated.
  bool GetExtensionCommand(extensions::Command* command);

  // Unregisters the accelerator for the extension action's command, if one
  // exists. If |only_if_removed| is true, then this will only unregister if the
  // command has been removed.
  void UnregisterCommand(bool only_if_removed);

  // Closes the currently-active menu, if needed. This is the case when there
  // is an active menu that wouldn't close automatically when a new one is
  // opened.
  // Returns true if a menu was closed, false otherwise.
  bool CloseActiveMenuIfNeeded();

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
  ToolbarActionViewDelegate* delegate_;

  // The object that will be used to get the browser action icon for us.
  // It may load the icon asynchronously (in which case the initial icon
  // returned by the factory will be transparent), so we have to observe it for
  // updates to the icon.
  ExtensionActionIconFactory icon_factory_;

  // The observer that we need to notify when the icon of the button has been
  // updated.
  ExtensionActionIconFactory::Observer* icon_observer_;

  // Responsible for running the menu.
  scoped_ptr<views::MenuRunner> menu_runner_;

  // The browser action's popup, if it is visible; NULL otherwise.
  ExtensionPopup* popup_;

  // The extension key binding accelerator this extension action is listening
  // for (to show the popup).
  scoped_ptr<ui::Accelerator> action_keybinding_;

  content::NotificationRegistrar registrar_;

  // If non-NULL, this is the next ExtensionActionViewController context menu
  // which wants to run once the current owner (this one) is done.
  base::Closure followup_context_menu_task_;

  base::WeakPtrFactory<ExtensionActionViewController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionActionViewController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_ACTION_VIEW_CONTROLLER_H_
