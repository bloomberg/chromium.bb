// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTEXT_MENU_MODEL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTEXT_MENU_MODEL_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "ui/base/models/simple_menu_model.h"

class Browser;
class ExtensionAction;
class Profile;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
class ContextMenuMatcher;
class ExtensionContextMenuModelTest;
}

// The context menu model for extension icons.
class ExtensionContextMenuModel
    : public base::RefCounted<ExtensionContextMenuModel>,
      public ui::SimpleMenuModel,
      public ui::SimpleMenuModel::Delegate,
      public extensions::ExtensionUninstallDialog::Delegate {
 public:
  enum MenuEntries {
    NAME = 0,
    CONFIGURE,
    TOGGLE_VISIBILITY,
    UNINSTALL,
    MANAGE,
    INSPECT_POPUP,
    ALWAYS_RUN
  };

  // Type of action the extension icon represents.
  enum ActionType { NO_ACTION = 0, BROWSER_ACTION, PAGE_ACTION };

  // The current visibility of the button; this can affect the "hide"/"show"
  // strings in the menu.
  enum ButtonVisibility {
    // The button is visible on the toolbar.
    VISIBLE,
    // The button is temporarily visible on the toolbar, as for showign a popup.
    TRANSITIVELY_VISIBLE,
    // The button is showed in the overflow menu.
    OVERFLOWED
  };

  // Delegate to handle showing an ExtensionAction popup.
  class PopupDelegate {
   public:
    // Called when the user selects the menu item which requests that the
    // popup be shown and inspected.
    // The delegate should know which popup to display.
    virtual void InspectPopup() = 0;

   protected:
    virtual ~PopupDelegate() {}
  };

  // Creates a menu model for the given extension. If
  // prefs::kExtensionsUIDeveloperMode is enabled then a menu item
  // will be shown for "Inspect Popup" which, when selected, will cause
  // ShowPopupForDevToolsWindow() to be called on |delegate|.
  ExtensionContextMenuModel(const extensions::Extension* extension,
                            Browser* browser,
                            ButtonVisibility visibility,
                            PopupDelegate* delegate);

  // Create a menu model for the given extension, without support
  // for the "Inspect Popup" command.
  ExtensionContextMenuModel(const extensions::Extension* extension,
                            Browser* browser);

  // SimpleMenuModel::Delegate overrides.
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) override;
  void ExecuteCommand(int command_id, int event_flags) override;

  // ExtensionUninstallDialog::Delegate:
  void ExtensionUninstallAccepted() override;
  void ExtensionUninstallCanceled() override;

 private:
  friend class base::RefCounted<ExtensionContextMenuModel>;
  friend class extensions::ExtensionContextMenuModelTest;

  ~ExtensionContextMenuModel() override;

  void InitMenu(const extensions::Extension* extension,
                ButtonVisibility button_visibility_);

  // Gets the extension we are displaying the menu for. Returns NULL if the
  // extension has been uninstalled and no longer exists.
  const extensions::Extension* GetExtension() const;

  // Returns the active web contents.
  content::WebContents* GetActiveWebContents() const;

  // Appends the extension's context menu items.
  void AppendExtensionItems();

  // A copy of the extension's id.
  std::string extension_id_;

  // The extension action of the extension we are displaying the menu for (if
  // it has one, otherwise NULL).
  ExtensionAction* extension_action_;

  Browser* browser_;

  Profile* profile_;

  // The delegate which handles the 'inspect popup' menu command (or NULL).
  PopupDelegate* delegate_;

  // The type of extension action to which this context menu is attached.
  ActionType action_type_;

  // Keeps track of the extension uninstall dialog.
  scoped_ptr<extensions::ExtensionUninstallDialog> extension_uninstall_dialog_;

  // Menu matcher for context menu items specified by the extension.
  scoped_ptr<extensions::ContextMenuMatcher> extension_items_;

  // Number of extension items in this menu. Used for testing.
  int extension_items_count_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionContextMenuModel);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTEXT_MENU_MODEL_H_
