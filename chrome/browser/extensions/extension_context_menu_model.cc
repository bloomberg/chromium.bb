// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_context_menu_model.h"

#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/active_script_controller.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/context_menu_params.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "ui/base/l10n/l10n_util.h"

using content::OpenURLParams;
using content::Referrer;
using content::WebContents;
using extensions::Extension;
using extensions::ExtensionActionAPI;
using extensions::ExtensionPrefs;
using extensions::MenuItem;
using extensions::MenuManager;

namespace {

// Returns true if the given |item| is of the given |type|.
bool MenuItemMatchesAction(ExtensionContextMenuModel::ActionType type,
                           const MenuItem* item) {
  if (type == ExtensionContextMenuModel::NO_ACTION)
    return false;

  const MenuItem::ContextList& contexts = item->contexts();

  if (contexts.Contains(MenuItem::ALL))
    return true;
  if (contexts.Contains(MenuItem::PAGE_ACTION) &&
      (type == ExtensionContextMenuModel::PAGE_ACTION))
    return true;
  if (contexts.Contains(MenuItem::BROWSER_ACTION) &&
      (type == ExtensionContextMenuModel::BROWSER_ACTION))
    return true;

  return false;
}

// Returns the id for the visibility command for the given |extension|, or -1
// if none should be shown.
int GetVisibilityStringId(Profile* profile, const Extension* extension) {
  DCHECK(profile);
  int string_id = -1;
  if (!extensions::FeatureSwitch::extension_action_redesign()->IsEnabled()) {
    // Without the toolbar redesign, we only show the visibility toggle for
    // browser actions, and only give the option to hide.
    if (extensions::ExtensionActionManager::Get(profile)->GetBrowserAction(
            *extension)) {
      string_id = IDS_EXTENSIONS_HIDE_BUTTON;
    }
  } else {
    // With the redesign, we display "show" or "hide" based on the icon's
    // visibility.
    bool visible = ExtensionActionAPI::GetBrowserActionVisibility(
                       ExtensionPrefs::Get(profile), extension->id());
    string_id =
        visible ? IDS_EXTENSIONS_HIDE_BUTTON : IDS_EXTENSIONS_SHOW_BUTTON;
  }
  return string_id;
}

}  // namespace

ExtensionContextMenuModel::ExtensionContextMenuModel(const Extension* extension,
                                                     Browser* browser,
                                                     PopupDelegate* delegate)
    : SimpleMenuModel(this),
      extension_id_(extension->id()),
      browser_(browser),
      profile_(browser->profile()),
      delegate_(delegate),
      action_type_(NO_ACTION),
      extension_items_count_(0) {
  InitMenu(extension);

  if (profile_->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode) &&
      delegate_) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItemWithStringId(INSPECT_POPUP, IDS_EXTENSION_ACTION_INSPECT_POPUP);
  }
}

ExtensionContextMenuModel::ExtensionContextMenuModel(const Extension* extension,
                                                     Browser* browser)
    : SimpleMenuModel(this),
      extension_id_(extension->id()),
      browser_(browser),
      profile_(browser->profile()),
      delegate_(NULL),
      action_type_(NO_ACTION),
      extension_items_count_(0) {
  InitMenu(extension);
}

bool ExtensionContextMenuModel::IsCommandIdChecked(int command_id) const {
  if (command_id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
      command_id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST)
    return extension_items_->IsCommandIdChecked(command_id);
  return false;
}

bool ExtensionContextMenuModel::IsCommandIdEnabled(int command_id) const {
  const Extension* extension = GetExtension();
  if (!extension)
    return false;

  if (command_id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
      command_id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST) {
    return extension_items_->IsCommandIdEnabled(command_id);
  } else if (command_id == CONFIGURE) {
    return extensions::OptionsPageInfo::HasOptionsPage(extension);
  } else if (command_id == NAME) {
    // The NAME links to the Homepage URL. If the extension doesn't have a
    // homepage, we just disable this menu item.
    return extensions::ManifestURL::GetHomepageURL(extension).is_valid();
  } else if (command_id == INSPECT_POPUP) {
    WebContents* web_contents = GetActiveWebContents();
    if (!web_contents)
      return false;

    return extension_action_ &&
        extension_action_->HasPopup(SessionTabHelper::IdForTab(web_contents));
  } else if (command_id == UNINSTALL) {
    // Some extension types can not be uninstalled.
    return extensions::ExtensionSystem::Get(
        profile_)->management_policy()->UserMayModifySettings(extension, NULL);
  }
  return true;
}

bool ExtensionContextMenuModel::GetAcceleratorForCommandId(
    int command_id, ui::Accelerator* accelerator) {
  return false;
}

void ExtensionContextMenuModel::ExecuteCommand(int command_id,
                                               int event_flags) {
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  if (command_id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
      command_id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST) {
    WebContents* web_contents =
        browser_->tab_strip_model()->GetActiveWebContents();
    DCHECK(extension_items_);
    extension_items_->ExecuteCommand(
        command_id, web_contents, content::ContextMenuParams());
    return;
  }

  switch (command_id) {
    case NAME: {
      OpenURLParams params(extensions::ManifestURL::GetHomepageURL(extension),
                           Referrer(), NEW_FOREGROUND_TAB,
                           ui::PAGE_TRANSITION_LINK, false);
      browser_->OpenURL(params);
      break;
    }
    case ALWAYS_RUN: {
      WebContents* web_contents = GetActiveWebContents();
      if (web_contents) {
        extensions::ActiveScriptController::GetForWebContents(web_contents)
            ->AlwaysRunOnVisibleOrigin(extension);
      }
      break;
    }
    case CONFIGURE:
      DCHECK(extensions::OptionsPageInfo::HasOptionsPage(extension));
      extensions::ExtensionTabUtil::OpenOptionsPage(extension, browser_);
      break;
    case TOGGLE_VISIBILITY: {
      ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_);
      bool visible = ExtensionActionAPI::GetBrowserActionVisibility(
                         prefs, extension->id());
      ExtensionActionAPI::SetBrowserActionVisibility(
          prefs, extension->id(), !visible);
      break;
    }
    case UNINSTALL: {
      AddRef();  // Balanced in Accepted() and Canceled()
      extension_uninstall_dialog_.reset(
          extensions::ExtensionUninstallDialog::Create(
              profile_, browser_->window()->GetNativeWindow(), this));
      extension_uninstall_dialog_->ConfirmUninstall(extension);
      break;
    }
    case MANAGE: {
      chrome::ShowExtensions(browser_, extension->id());
      break;
    }
    case INSPECT_POPUP: {
      delegate_->InspectPopup();
      break;
    }
    default:
     NOTREACHED() << "Unknown option";
     break;
  }
}

void ExtensionContextMenuModel::ExtensionUninstallAccepted() {
  if (GetExtension()) {
    extensions::ExtensionSystem::Get(profile_)
        ->extension_service()
        ->UninstallExtension(extension_id_,
                             extensions::UNINSTALL_REASON_USER_INITIATED,
                             base::Bind(&base::DoNothing),
                             NULL);
  }
  Release();
}

void ExtensionContextMenuModel::ExtensionUninstallCanceled() {
  Release();
}

ExtensionContextMenuModel::~ExtensionContextMenuModel() {}

void ExtensionContextMenuModel::InitMenu(const Extension* extension) {
  DCHECK(extension);

  extensions::ExtensionActionManager* extension_action_manager =
      extensions::ExtensionActionManager::Get(profile_);
  extension_action_ = extension_action_manager->GetBrowserAction(*extension);
  if (!extension_action_) {
    extension_action_ = extension_action_manager->GetPageAction(*extension);
    if (extension_action_)
      action_type_ = PAGE_ACTION;
  } else {
    action_type_ = BROWSER_ACTION;
  }

  extension_items_.reset(new extensions::ContextMenuMatcher(
      profile_, this, this, base::Bind(MenuItemMatchesAction, action_type_)));

  std::string extension_name = extension->name();
  // Ampersands need to be escaped to avoid being treated like
  // mnemonics in the menu.
  base::ReplaceChars(extension_name, "&", "&&", &extension_name);
  AddItem(NAME, base::UTF8ToUTF16(extension_name));
  AppendExtensionItems();
  AddSeparator(ui::NORMAL_SEPARATOR);

  // Add the "Always Allow" item for adding persisted permissions for script
  // injections if there is an active action for this extension. Note that this
  // will add it to *all* extension action context menus, not just the one
  // attached to the script injection request icon, but that's okay.
  WebContents* web_contents = GetActiveWebContents();
  if (web_contents &&
      extensions::ActiveScriptController::GetForWebContents(web_contents)
          ->WantsToRun(extension)) {
    AddItemWithStringId(ALWAYS_RUN, IDS_EXTENSIONS_ALWAYS_RUN);
  }

  AddItemWithStringId(CONFIGURE, IDS_EXTENSIONS_OPTIONS_MENU_ITEM);
  AddItem(UNINSTALL, l10n_util::GetStringUTF16(IDS_EXTENSIONS_UNINSTALL));

  // Add a toggle visibility (show/hide) if the extension icon is shown on the
  // toolbar.
  int visibility_string_id = GetVisibilityStringId(profile_, extension);
  if (visibility_string_id != -1)
    AddItemWithStringId(TOGGLE_VISIBILITY, visibility_string_id);

  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItemWithStringId(MANAGE, IDS_MANAGE_EXTENSION);
}

const Extension* ExtensionContextMenuModel::GetExtension() const {
  return extensions::ExtensionRegistry::Get(profile_)
      ->enabled_extensions()
      .GetByID(extension_id_);
}

void ExtensionContextMenuModel::AppendExtensionItems() {
  extension_items_->Clear();

  MenuManager* menu_manager = MenuManager::Get(profile_);
  if (!menu_manager ||
      !menu_manager->MenuItems(MenuItem::ExtensionKey(extension_id_)))
    return;

  AddSeparator(ui::NORMAL_SEPARATOR);

  extension_items_count_ = 0;
  extension_items_->AppendExtensionItems(MenuItem::ExtensionKey(extension_id_),
                                         base::string16(),
                                         &extension_items_count_,
                                         true);  // is_action_menu
}

content::WebContents* ExtensionContextMenuModel::GetActiveWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}
