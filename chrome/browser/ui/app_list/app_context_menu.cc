// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_context_menu.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/management_policy.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "content/public/common/context_menu_params.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#endif

using extensions::Extension;

namespace app_list {

namespace {

enum CommandId {
  LAUNCH_NEW = 100,
  TOGGLE_PIN,
  CREATE_SHORTCUTS,
  OPTIONS,
  UNINSTALL,
  DETAILS,
  MENU_NEW_WINDOW,
  MENU_NEW_INCOGNITO_WINDOW,
  // Order matters in LAUNCHER_TYPE_xxxx and must match LaunchType.
  LAUNCH_TYPE_START = 200,
  LAUNCH_TYPE_PINNED_TAB = LAUNCH_TYPE_START,
  LAUNCH_TYPE_REGULAR_TAB,
  LAUNCH_TYPE_FULLSCREEN,
  LAUNCH_TYPE_WINDOW,
  LAUNCH_TYPE_LAST,
};

// ExtensionUninstaller runs the extension uninstall flow. It shows the
// extension uninstall dialog and wait for user to confirm or cancel the
// uninstall.
class ExtensionUninstaller : public ExtensionUninstallDialog::Delegate {
 public:
  ExtensionUninstaller(Profile* profile,
                       const std::string& extension_id,
                       AppListControllerDelegate* controller)
      : profile_(profile),
        app_id_(extension_id),
        controller_(controller) {
  }

  void Run() {
    const Extension* extension =
        extensions::ExtensionSystem::Get(profile_)->extension_service()->
            GetExtensionById(app_id_, true);
    if (!extension) {
      CleanUp();
      return;
    }
    controller_->OnShowExtensionPrompt();
    dialog_.reset(ExtensionUninstallDialog::Create(profile_, NULL, this));
    dialog_->ConfirmUninstall(extension);
  }

 private:
  // Overridden from ExtensionUninstallDialog::Delegate:
  virtual void ExtensionUninstallAccepted() OVERRIDE {
    ExtensionService* service =
        extensions::ExtensionSystem::Get(profile_)->extension_service();
    const Extension* extension = service->GetInstalledExtension(app_id_);
    if (extension) {
      service->UninstallExtension(app_id_,
                                  false, /* external_uninstall*/
                                  NULL);
    }
    controller_->OnCloseExtensionPrompt();
    CleanUp();
  }

  virtual void ExtensionUninstallCanceled() OVERRIDE {
    controller_->OnCloseExtensionPrompt();
    CleanUp();
  }

  void CleanUp() {
    delete this;
  }

  Profile* profile_;
  std::string app_id_;
  AppListControllerDelegate* controller_;
  scoped_ptr<ExtensionUninstallDialog> dialog_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUninstaller);
};

extensions::ExtensionPrefs::LaunchType GetExtensionLaunchType(
    Profile* profile,
    const Extension* extension) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  return service->extension_prefs()->
      GetLaunchType(extension, extensions::ExtensionPrefs::LAUNCH_DEFAULT);
}

void SetExtensionLaunchType(
    Profile* profile,
    const std::string& extension_id,
    extensions::ExtensionPrefs::LaunchType launch_type) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  service->extension_prefs()->SetLaunchType(extension_id, launch_type);
}

bool MenuItemHasLauncherContext(const extensions::MenuItem* item) {
  return item->contexts().Contains(extensions::MenuItem::LAUNCHER);
}

}  // namespace

AppContextMenu::AppContextMenu(AppContextMenuDelegate* delegate,
                               Profile* profile,
                               const std::string& app_id,
                               AppListControllerDelegate* controller,
                               bool is_platform_app,
                               bool is_search_result)
    : delegate_(delegate),
      profile_(profile),
      app_id_(app_id),
      controller_(controller),
      is_platform_app_(is_platform_app),
      is_search_result_(is_search_result) {
}

AppContextMenu::~AppContextMenu() {}

ui::MenuModel* AppContextMenu::GetMenuModel() {
  const Extension* extension = GetExtension();
  if (!extension)
    return NULL;

  if (menu_model_.get())
    return menu_model_.get();

  menu_model_.reset(new ui::SimpleMenuModel(this));

  if (app_id_ == extension_misc::kChromeAppId) {
    menu_model_->AddItemWithStringId(
        MENU_NEW_WINDOW,
        IDS_APP_LIST_NEW_WINDOW);
    if (!profile_->IsOffTheRecord()) {
      menu_model_->AddItemWithStringId(
          MENU_NEW_INCOGNITO_WINDOW,
          IDS_APP_LIST_NEW_INCOGNITO_WINDOW);
    }
  } else {
    extension_menu_items_.reset(new extensions::ContextMenuMatcher(
        profile_, this, menu_model_.get(),
        base::Bind(MenuItemHasLauncherContext)));

    if (!is_platform_app_)
      menu_model_->AddItem(LAUNCH_NEW, string16());

    int index = 0;
    extension_menu_items_->AppendExtensionItems(app_id_, string16(),
                                                &index);

#if defined(USE_ASH)
    // Always show Pin/Unpin option for ash.
    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_->AddItemWithStringId(
        TOGGLE_PIN,
        controller_->IsAppPinned(app_id_) ?
            IDS_APP_LIST_CONTEXT_MENU_UNPIN :
            IDS_APP_LIST_CONTEXT_MENU_PIN);
#endif

    if (controller_->CanDoCreateShortcutsFlow(is_platform_app_)) {
      menu_model_->AddItemWithStringId(CREATE_SHORTCUTS,
                                       IDS_NEW_TAB_APP_CREATE_SHORTCUT);
    }

    if (!is_platform_app_) {
      menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
      menu_model_->AddCheckItemWithStringId(
          LAUNCH_TYPE_REGULAR_TAB,
          IDS_APP_CONTEXT_MENU_OPEN_REGULAR);
      menu_model_->AddCheckItemWithStringId(
          LAUNCH_TYPE_PINNED_TAB,
          IDS_APP_CONTEXT_MENU_OPEN_PINNED);
#if defined(USE_ASH)
      if (!ash::Shell::IsForcedMaximizeMode())
#endif
      {
#if defined(OS_MACOSX)
        // Mac does not support standalone web app browser windows or maximize.
        menu_model_->AddCheckItemWithStringId(
            LAUNCH_TYPE_FULLSCREEN,
            IDS_APP_CONTEXT_MENU_OPEN_FULLSCREEN);
#else
        menu_model_->AddCheckItemWithStringId(
            LAUNCH_TYPE_WINDOW,
            IDS_APP_CONTEXT_MENU_OPEN_WINDOW);
        // Even though the launch type is Full Screen it is more accurately
        // described as Maximized in Ash.
        menu_model_->AddCheckItemWithStringId(
            LAUNCH_TYPE_FULLSCREEN,
            IDS_APP_CONTEXT_MENU_OPEN_MAXIMIZED);
#endif
      }
      menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
      menu_model_->AddItemWithStringId(OPTIONS, IDS_NEW_TAB_APP_OPTIONS);
    }

    menu_model_->AddItemWithStringId(DETAILS, IDS_NEW_TAB_APP_DETAILS);
    menu_model_->AddItemWithStringId(
        UNINSTALL,
        is_platform_app_ ? IDS_APP_LIST_UNINSTALL_ITEM
                         : IDS_EXTENSIONS_UNINSTALL);
  }

  return menu_model_.get();
}

const Extension* AppContextMenu::GetExtension() const {
  const ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  const Extension* extension = service->GetInstalledExtension(app_id_);
  return extension;
}

void AppContextMenu::ShowExtensionOptions() {
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  chrome::NavigateParams params(
      profile_,
      extensions::ManifestURL::GetOptionsPage(extension),
      content::PAGE_TRANSITION_LINK);
  chrome::Navigate(&params);
}

void AppContextMenu::ShowExtensionDetails() {
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  const GURL url = extensions::ManifestURL::GetDetailsURL(extension);
  DCHECK_NE(url, GURL::EmptyGURL());

  const std::string source = AppListControllerDelegate::AppListSourceToString(
      is_search_result_ ?
          AppListControllerDelegate::LAUNCH_FROM_APP_LIST_SEARCH :
          AppListControllerDelegate::LAUNCH_FROM_APP_LIST);
  chrome::NavigateParams params(
      profile_,
      net::AppendQueryParameter(url,
                                extension_urls::kWebstoreSourceField,
                                source),
      content::PAGE_TRANSITION_LINK);
  chrome::Navigate(&params);
}

void AppContextMenu::StartExtensionUninstall() {
  // ExtensionUninstall deletes itself when done or aborted.
  ExtensionUninstaller* uninstaller = new ExtensionUninstaller(profile_,
                                                               app_id_,
                                                               controller_);
  uninstaller->Run();
}

bool AppContextMenu::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == TOGGLE_PIN || command_id == LAUNCH_NEW;
}

string16 AppContextMenu::GetLabelForCommandId(int command_id) const {
  if (command_id == TOGGLE_PIN) {
    return controller_->IsAppPinned(app_id_) ?
        l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_UNPIN) :
        l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_PIN);
  } else if (command_id == LAUNCH_NEW) {
#if defined(OS_MACOSX)
    // Even fullscreen windows launch in a browser tab on Mac.
    const bool launches_in_tab = true;
#else
    const bool launches_in_tab = IsCommandIdChecked(LAUNCH_TYPE_PINNED_TAB) ||
        IsCommandIdChecked(LAUNCH_TYPE_REGULAR_TAB);
#endif
    return launches_in_tab ?
        l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_NEW_TAB) :
        l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW);
  } else {
    NOTREACHED();
    return string16();
  }
}

bool AppContextMenu::IsCommandIdChecked(int command_id) const {
  if (command_id >= LAUNCH_TYPE_START && command_id < LAUNCH_TYPE_LAST) {
    return static_cast<int>(GetExtensionLaunchType(profile_, GetExtension())) +
        LAUNCH_TYPE_START == command_id;
  } else if (command_id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
             command_id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST) {
    return extension_menu_items_->IsCommandIdChecked(command_id);
  }
  return false;
}

bool AppContextMenu::IsCommandIdEnabled(int command_id) const {
  if (command_id == TOGGLE_PIN) {
    return controller_->CanPin();
  } else if (command_id == OPTIONS) {
    const ExtensionService* service =
        extensions::ExtensionSystem::Get(profile_)->extension_service();
    const Extension* extension = GetExtension();
    return service->IsExtensionEnabledForLauncher(app_id_) &&
           extension &&
           !extensions::ManifestURL::GetOptionsPage(extension).is_empty();
  } else if (command_id == UNINSTALL) {
    const Extension* extension = GetExtension();
    const extensions::ManagementPolicy* policy =
        extensions::ExtensionSystem::Get(profile_)->management_policy();
    return extension &&
           policy->UserMayModifySettings(extension, NULL);
  } else if (command_id == DETAILS) {
    const Extension* extension = GetExtension();
    return extension && extension->from_webstore();
  } else if (command_id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
             command_id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST) {
    return extension_menu_items_->IsCommandIdEnabled(command_id);
  } else if (command_id == MENU_NEW_WINDOW) {
    // "Normal" windows are not allowed when incognito is enforced.
    return IncognitoModePrefs::GetAvailability(profile_->GetPrefs()) !=
        IncognitoModePrefs::FORCED;
  } else if (command_id == MENU_NEW_INCOGNITO_WINDOW) {
    // Incognito windows are not allowed when incognito is disabled.
    return IncognitoModePrefs::GetAvailability(profile_->GetPrefs()) !=
        IncognitoModePrefs::DISABLED;
  }
  return true;
}

bool AppContextMenu::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* acclelrator) {
  return false;
}

void AppContextMenu::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == LAUNCH_NEW) {
    delegate_->ExecuteLaunchCommand(event_flags);
  } else if (command_id == TOGGLE_PIN && controller_->CanPin()) {
    if (controller_->IsAppPinned(app_id_))
      controller_->UnpinApp(app_id_);
    else
      controller_->PinApp(app_id_);
  } else if (command_id == CREATE_SHORTCUTS) {
    controller_->DoCreateShortcutsFlow(profile_, app_id_);
  } else if (command_id >= LAUNCH_TYPE_START &&
             command_id < LAUNCH_TYPE_LAST) {
    SetExtensionLaunchType(profile_,
                           app_id_,
                           static_cast<extensions::ExtensionPrefs::LaunchType>(
                               command_id - LAUNCH_TYPE_START));
  } else if (command_id == OPTIONS) {
    ShowExtensionOptions();
  } else if (command_id == UNINSTALL) {
    StartExtensionUninstall();
  } else if (command_id == DETAILS) {
    ShowExtensionDetails();
  } else if (command_id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
             command_id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST) {
    extension_menu_items_->ExecuteCommand(command_id, NULL,
                                          content::ContextMenuParams());
  } else if (command_id == MENU_NEW_WINDOW) {
    controller_->CreateNewWindow(profile_, false);
  } else if (command_id == MENU_NEW_INCOGNITO_WINDOW) {
    controller_->CreateNewWindow(profile_, true);
  }
}

}  // namespace app_list
