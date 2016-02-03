// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_context_menu_model.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/active_script_controller.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/context_menu_params.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/manifest_url_handlers.h"
#include "grit/components_scaled_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace extensions {

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
int GetVisibilityStringId(
    Profile* profile,
    const Extension* extension,
    ExtensionContextMenuModel::ButtonVisibility button_visibility) {
  DCHECK(profile);
  int string_id = -1;
  if (!FeatureSwitch::extension_action_redesign()->IsEnabled()) {
    // Without the toolbar redesign, we only show the visibility toggle for
    // browser actions, and only give the option to hide.
    if (ExtensionActionManager::Get(profile)->GetBrowserAction(*extension)) {
      string_id = IDS_EXTENSIONS_HIDE_BUTTON;
    }
  } else {
    // With the redesign, we display "show" or "hide" based on the icon's
    // visibility, and can have "transitively shown" buttons that are shown
    // only while the button has a popup or menu visible.
    switch (button_visibility) {
      case (ExtensionContextMenuModel::VISIBLE):
        string_id = IDS_EXTENSIONS_HIDE_BUTTON_IN_MENU;
        break;
      case (ExtensionContextMenuModel::TRANSITIVELY_VISIBLE):
        string_id = IDS_EXTENSIONS_KEEP_BUTTON_IN_TOOLBAR;
        break;
      case (ExtensionContextMenuModel::OVERFLOWED):
        string_id = IDS_EXTENSIONS_SHOW_BUTTON_IN_TOOLBAR;
        break;
    }
  }
  return string_id;
}

// Returns true if the given |extension| is required to remain installed by
// policy.
bool IsExtensionRequiredByPolicy(const Extension* extension,
                                 Profile* profile) {
  ManagementPolicy* policy = ExtensionSystem::Get(profile)->management_policy();
  return !policy->UserMayModifySettings(extension, nullptr) ||
         policy->MustRemainInstalled(extension, nullptr);
}

// A stub for the uninstall dialog.
// TODO(devlin): Ideally, we would just have the uninstall dialog take a
// base::Callback, but that's a bunch of churn.
class UninstallDialogHelper : public ExtensionUninstallDialog::Delegate {
 public:
  // Kicks off the asynchronous process to confirm and uninstall the given
  // |extension|.
  static void UninstallExtension(Browser* browser, const Extension* extension) {
    UninstallDialogHelper* helper = new UninstallDialogHelper();
    helper->BeginUninstall(browser, extension);
  }

 private:
  // This class handles its own lifetime.
  UninstallDialogHelper() {}
  ~UninstallDialogHelper() override {}

  void BeginUninstall(Browser* browser, const Extension* extension) {
    uninstall_dialog_.reset(ExtensionUninstallDialog::Create(
        browser->profile(), browser->window()->GetNativeWindow(), this));
    uninstall_dialog_->ConfirmUninstall(extension,
                                        UNINSTALL_REASON_USER_INITIATED,
                                        UNINSTALL_SOURCE_TOOLBAR_CONTEXT_MENU);
  }

  // ExtensionUninstallDialog::Delegate:
  void OnExtensionUninstallDialogClosed(bool did_start_uninstall,
                                        const base::string16& error) override {
    delete this;
  }

  scoped_ptr<ExtensionUninstallDialog> uninstall_dialog_;

  DISALLOW_COPY_AND_ASSIGN(UninstallDialogHelper);
};

}  // namespace

ExtensionContextMenuModel::ExtensionContextMenuModel(
    const Extension* extension,
    Browser* browser,
    ButtonVisibility button_visibility,
    PopupDelegate* delegate)
    : SimpleMenuModel(this),
      extension_id_(extension->id()),
      is_component_(Manifest::IsComponentLocation(extension->location())),
      browser_(browser),
      profile_(browser->profile()),
      delegate_(delegate),
      action_type_(NO_ACTION),
      button_visibility_(button_visibility) {
  InitMenu(extension, button_visibility);
}

bool ExtensionContextMenuModel::IsCommandIdChecked(int command_id) const {
  const Extension* extension = GetExtension();
  if (!extension)
    return false;

  if (command_id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
      command_id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST)
    return extension_items_->IsCommandIdChecked(command_id);

  if (command_id == PAGE_ACCESS_RUN_ON_CLICK ||
      command_id == PAGE_ACCESS_RUN_ON_SITE ||
      command_id == PAGE_ACCESS_RUN_ON_ALL_SITES) {
    content::WebContents* web_contents = GetActiveWebContents();
    return web_contents &&
           GetCurrentPageAccess(extension, web_contents) == command_id;
  }

  return false;
}

bool ExtensionContextMenuModel::IsCommandIdEnabled(int command_id) const {
  const Extension* extension = GetExtension();
  if (!extension)
    return false;

  if (command_id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
      command_id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST) {
    return extension_items_->IsCommandIdEnabled(command_id);
  }

  switch (command_id) {
    case NAME:
      // The NAME links to the Homepage URL. If the extension doesn't have a
      // homepage, we just disable this menu item. We also disable for component
      // extensions, because it doesn't make sense to link to a webstore page or
      // chrome://extensions.
      return ManifestURL::GetHomepageURL(extension).is_valid() &&
             !is_component_;
    case CONFIGURE:
      return OptionsPageInfo::HasOptionsPage(extension);
    case INSPECT_POPUP: {
      content::WebContents* web_contents = GetActiveWebContents();
      return web_contents && extension_action_ &&
             extension_action_->HasPopup(
                 SessionTabHelper::IdForTab(web_contents));
    }
    case UNINSTALL:
      return !IsExtensionRequiredByPolicy(extension, profile_);
    // The following, if they are present, are always enabled.
    case TOGGLE_VISIBILITY:
    case MANAGE:
    case PAGE_ACCESS_SUBMENU:
    case PAGE_ACCESS_RUN_ON_CLICK:
    case PAGE_ACCESS_RUN_ON_SITE:
    case PAGE_ACCESS_RUN_ON_ALL_SITES:
      return true;
    default:
      NOTREACHED() << "Unknown command" << command_id;
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
    DCHECK(extension_items_);
    extension_items_->ExecuteCommand(command_id, GetActiveWebContents(),
                                     content::ContextMenuParams());
    return;
  }

  switch (command_id) {
    case NAME: {
      content::OpenURLParams params(ManifestURL::GetHomepageURL(extension),
                                    content::Referrer(), NEW_FOREGROUND_TAB,
                                    ui::PAGE_TRANSITION_LINK, false);
      browser_->OpenURL(params);
      break;
    }
    case CONFIGURE:
      DCHECK(OptionsPageInfo::HasOptionsPage(extension));
      ExtensionTabUtil::OpenOptionsPage(extension, browser_);
      break;
    case TOGGLE_VISIBILITY: {
      bool currently_visible = button_visibility_ == VISIBLE;
      // Without the toolbar redesign turned on, action visibility refers to
      // any action presence in the toolbar, independent of whether the action
      // is visible or overflowed. So any action present is considered visible.
      if (!FeatureSwitch::extension_action_redesign()->IsEnabled())
        currently_visible = true;
      ToolbarActionsModel::Get(browser_->profile())
          ->SetActionVisibility(extension->id(), !currently_visible);
      break;
    }
    case UNINSTALL: {
      UninstallDialogHelper::UninstallExtension(browser_, extension);
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
    case PAGE_ACCESS_RUN_ON_CLICK:
    case PAGE_ACCESS_RUN_ON_SITE:
    case PAGE_ACCESS_RUN_ON_ALL_SITES:
      HandlePageAccessCommand(command_id, extension);
      break;
    default:
     NOTREACHED() << "Unknown option";
     break;
  }
}

ExtensionContextMenuModel::~ExtensionContextMenuModel() {}

void ExtensionContextMenuModel::InitMenu(const Extension* extension,
                                         ButtonVisibility button_visibility) {
  DCHECK(extension);

  extension_action_ =
      ExtensionActionManager::Get(profile_)->GetExtensionAction(*extension);
  if (extension_action_) {
    action_type_ = extension_action_->action_type() == ActionInfo::TYPE_PAGE
                       ? PAGE_ACTION
                       : BROWSER_ACTION;
  }

  extension_items_.reset(new ContextMenuMatcher(
      profile_, this, this, base::Bind(MenuItemMatchesAction, action_type_)));

  std::string extension_name = extension->name();
  // Ampersands need to be escaped to avoid being treated like
  // mnemonics in the menu.
  base::ReplaceChars(extension_name, "&", "&&", &extension_name);
  AddItem(NAME, base::UTF8ToUTF16(extension_name));
  AppendExtensionItems();
  AddSeparator(ui::NORMAL_SEPARATOR);

  CreatePageAccessSubmenu(extension);

  if (!is_component_ || OptionsPageInfo::HasOptionsPage(extension))
    AddItemWithStringId(CONFIGURE, IDS_EXTENSIONS_OPTIONS_MENU_ITEM);

  if (!is_component_) {
    bool is_required_by_policy =
        IsExtensionRequiredByPolicy(extension, profile_);
    int message_id = is_required_by_policy ?
        IDS_EXTENSIONS_INSTALLED_BY_ADMIN : IDS_EXTENSIONS_UNINSTALL;
    AddItem(UNINSTALL, l10n_util::GetStringUTF16(message_id));
    if (is_required_by_policy) {
      int uninstall_index = GetIndexOfCommandId(UNINSTALL);
      SetIcon(uninstall_index,
              ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                  IDR_OMNIBOX_HTTPS_POLICY_WARNING));
    }
  }

  // Add a toggle visibility (show/hide) if the extension icon is shown on the
  // toolbar.
  int visibility_string_id =
      GetVisibilityStringId(profile_, extension, button_visibility);
  if (visibility_string_id != -1)
    AddItemWithStringId(TOGGLE_VISIBILITY, visibility_string_id);

  if (!is_component_) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItemWithStringId(MANAGE, IDS_MANAGE_EXTENSION);
  }

  if (profile_->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode) &&
      delegate_ && !is_component_) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItemWithStringId(INSPECT_POPUP, IDS_EXTENSION_ACTION_INSPECT_POPUP);
  }
}

const Extension* ExtensionContextMenuModel::GetExtension() const {
  return ExtensionRegistry::Get(profile_)->enabled_extensions().GetByID(
      extension_id_);
}

void ExtensionContextMenuModel::AppendExtensionItems() {
  MenuManager* menu_manager = MenuManager::Get(profile_);
  if (!menu_manager ||  // Null in unit tests
      !menu_manager->MenuItems(MenuItem::ExtensionKey(extension_id_)))
    return;

  AddSeparator(ui::NORMAL_SEPARATOR);

  int index = 0;
  extension_items_->AppendExtensionItems(MenuItem::ExtensionKey(extension_id_),
                                         base::string16(), &index,
                                         true);  // is_action_menu
}

ExtensionContextMenuModel::MenuEntries
ExtensionContextMenuModel::GetCurrentPageAccess(
    const Extension* extension,
    content::WebContents* web_contents) const {
  ScriptingPermissionsModifier modifier(profile_, extension);
  DCHECK(modifier.HasAffectedExtension());
  if (util::AllowedScriptingOnAllUrls(extension->id(), profile_))
    return PAGE_ACCESS_RUN_ON_ALL_SITES;
  if (modifier.HasGrantedHostPermission(
          GetActiveWebContents()->GetLastCommittedURL()))
    return PAGE_ACCESS_RUN_ON_SITE;
  return PAGE_ACCESS_RUN_ON_CLICK;
}

void ExtensionContextMenuModel::CreatePageAccessSubmenu(
    const Extension* extension) {
  content::WebContents* web_contents = GetActiveWebContents();
  if (!web_contents ||
      !ScriptingPermissionsModifier(profile_, extension)
           .HasAffectedExtension()) {
    return;
  }
  page_access_submenu_.reset(new ui::SimpleMenuModel(this));
  const int kRadioGroup = 0;
  page_access_submenu_->AddRadioItemWithStringId(
      PAGE_ACCESS_RUN_ON_CLICK,
      IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_RUN_ON_CLICK, kRadioGroup);
  page_access_submenu_->AddRadioItem(
      PAGE_ACCESS_RUN_ON_SITE,
      l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_RUN_ON_SITE,
          base::UTF8ToUTF16(
              web_contents->GetLastCommittedURL().GetOrigin().spec())),
      kRadioGroup);
  page_access_submenu_->AddRadioItemWithStringId(
      PAGE_ACCESS_RUN_ON_ALL_SITES,
      IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS_RUN_ON_ALL_SITES, kRadioGroup);

  AddSubMenuWithStringId(PAGE_ACCESS_SUBMENU,
                         IDS_EXTENSIONS_CONTEXT_MENU_PAGE_ACCESS,
                         page_access_submenu_.get());
}

void ExtensionContextMenuModel::HandlePageAccessCommand(
    int command_id,
    const Extension* extension) const {
  content::WebContents* web_contents = GetActiveWebContents();
  if (!web_contents)
    return;

  MenuEntries current_access = GetCurrentPageAccess(extension, web_contents);
  if (command_id == current_access)
    return;

  const GURL& url = web_contents->GetLastCommittedURL();
  ScriptingPermissionsModifier modifier(profile_, extension);
  switch (command_id) {
    case PAGE_ACCESS_RUN_ON_CLICK:
      if (current_access == PAGE_ACCESS_RUN_ON_ALL_SITES)
        util::SetAllowedScriptingOnAllUrls(extension->id(), profile_, false);
      if (modifier.HasGrantedHostPermission(url))
        modifier.RemoveGrantedHostPermission(url);
      break;
    case PAGE_ACCESS_RUN_ON_SITE:
      if (current_access == PAGE_ACCESS_RUN_ON_ALL_SITES)
        util::SetAllowedScriptingOnAllUrls(extension->id(), profile_, false);
      if (!modifier.HasGrantedHostPermission(url))
        modifier.GrantHostPermission(url);
      break;
    case PAGE_ACCESS_RUN_ON_ALL_SITES:
      util::SetAllowedScriptingOnAllUrls(extension->id(), profile_, true);
      break;
    default:
      NOTREACHED();
  }

  if (command_id == PAGE_ACCESS_RUN_ON_SITE ||
      command_id == PAGE_ACCESS_RUN_ON_ALL_SITES) {
    ActiveScriptController* controller =
        ActiveScriptController::GetForWebContents(web_contents);
    if (controller && controller->WantsToRun(extension))
      controller->OnClicked(extension);
  }
}

content::WebContents* ExtensionContextMenuModel::GetActiveWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

}  // namespace extensions
