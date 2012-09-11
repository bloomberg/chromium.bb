// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/extension_app_item.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/management_policy.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/extensions/api/rtc_private/rtc_private_api.h"
#endif

using extensions::Extension;

namespace {

enum CommandId {
  LAUNCH = 100,
  TOGGLE_PIN,
  OPTIONS,
  UNINSTALL,
  DETAILS,
  // Order matters in LAUNCHER_TYPE_xxxx and must match LaunchType.
  LAUNCH_TYPE_START = 200,
  LAUNCH_TYPE_PINNED_TAB = LAUNCH_TYPE_START,
  LAUNCH_TYPE_REGULAR_TAB,
  LAUNCH_TYPE_FULLSCREEN,
  LAUNCH_TYPE_WINDOW,
  LAUNCH_TYPE_LAST,
};

// ExtensionUninstaller decouples ExtensionAppItem from the extension uninstall
// flow. It shows extension uninstall dialog and wait for user to confirm or
// cancel the uninstall.
class ExtensionUninstaller : public ExtensionUninstallDialog::Delegate {
 public:
  ExtensionUninstaller(Profile* profile,
                       const std::string& extension_id)
      : profile_(profile),
        extension_id_(extension_id) {
  }

  void Run() {
    const Extension* extension =
        profile_->GetExtensionService()->GetExtensionById(extension_id_, true);
    if (!extension) {
      CleanUp();
      return;
    }

    ExtensionUninstallDialog* dialog =
        ExtensionUninstallDialog::Create(NULL, this);
    dialog->ConfirmUninstall(extension);
  }

 private:
  // Overridden from ExtensionUninstallDialog::Delegate:
  virtual void ExtensionUninstallAccepted() OVERRIDE {
    ExtensionService* service = profile_->GetExtensionService();
    const Extension* extension = service->GetExtensionById(extension_id_, true);
    if (extension) {
      service->UninstallExtension(extension_id_,
                                  false, /* external_uninstall*/
                                  NULL);
    }

    CleanUp();
  }

  virtual void ExtensionUninstallCanceled() OVERRIDE {
    CleanUp();
  }

  void CleanUp() {
    delete this;
  }

  Profile* profile_;
  std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUninstaller);
};

extensions::ExtensionPrefs::LaunchType GetExtensionLaunchType(
    Profile* profile,
    const std::string& extension_id) {
  return profile->GetExtensionService()->extension_prefs()->GetLaunchType(
      extension_id, extensions::ExtensionPrefs::LAUNCH_DEFAULT);
}

void SetExtensionLaunchType(
    Profile* profile,
    const std::string& extension_id,
    extensions::ExtensionPrefs::LaunchType launch_type) {
  profile->GetExtensionService()->extension_prefs()->SetLaunchType(
      extension_id, launch_type);
}

bool IsExtensionEnabled(Profile* profile, const std::string& extension_id) {
  ExtensionService* service = profile->GetExtensionService();
  return service->IsExtensionEnabled(extension_id) &&
      !service->GetTerminatedExtension(extension_id);
}

}  // namespace

ExtensionAppItem::ExtensionAppItem(Profile* profile,
                                   const Extension* extension,
                                   AppListController* controller)
    : ChromeAppListItem(TYPE_APP),
      profile_(profile),
      extension_id_(extension->id()),
      controller_(controller) {
  SetTitle(extension->name());
  LoadImage(extension);
}

ExtensionAppItem::~ExtensionAppItem() {
}

const Extension* ExtensionAppItem::GetExtension() const {
  const Extension* extension =
    profile_->GetExtensionService()->GetInstalledExtension(extension_id_);
  return extension;
}

bool ExtensionAppItem::IsTalkExtension() const {
  // Test most likely version first.
  return extension_id_ == extension_misc::kTalkExtensionId ||
      extension_id_ == extension_misc::kTalkBetaExtensionId ||
      extension_id_ == extension_misc::kTalkAlphaExtensionId ||
      extension_id_ == extension_misc::kTalkDebugExtensionId;
}

void ExtensionAppItem::LoadImage(const Extension* extension) {
  icon_.reset(new extensions::IconImage(
      extension,
      extension->icons(),
      extension_misc::EXTENSION_ICON_MEDIUM,
      Extension::GetDefaultIcon(true),
      this));
  SetIcon(icon_->image_skia());
}

void ExtensionAppItem::ShowExtensionOptions() {
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  chrome::NavigateParams params(profile_,
                                extension->options_url(),
                                content::PAGE_TRANSITION_LINK);
  chrome::Navigate(&params);
}

void ExtensionAppItem::ShowExtensionDetails() {
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  chrome::NavigateParams params(profile_,
                                extension->details_url(),
                                content::PAGE_TRANSITION_LINK);
  chrome::Navigate(&params);
}

void ExtensionAppItem::StartExtensionUninstall() {
  // ExtensionUninstall deletes itself when done or aborted.
  ExtensionUninstaller* uninstaller = new ExtensionUninstaller(profile_,
                                                               extension_id_);
  uninstaller->Run();
}

void ExtensionAppItem::OnExtensionIconImageChanged(
    extensions::IconImage* image) {
  DCHECK(icon_.get() == image);
  SetIcon(icon_->image_skia());
}

bool ExtensionAppItem::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == TOGGLE_PIN;
}

string16 ExtensionAppItem::GetLabelForCommandId(int command_id) const {
  if (command_id == TOGGLE_PIN) {
    return controller_->IsAppPinned(extension_id_) ?
        l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_UNPIN) :
        l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_PIN);
  } else {
    NOTREACHED();
    return string16();
  }
}

bool ExtensionAppItem::IsCommandIdChecked(int command_id) const {
  if (command_id >= LAUNCH_TYPE_START && command_id < LAUNCH_TYPE_LAST) {
    return static_cast<int>(GetExtensionLaunchType(profile_, extension_id_)) +
        LAUNCH_TYPE_START == command_id;
  }
  return false;
}

bool ExtensionAppItem::IsCommandIdEnabled(int command_id) const {
  if (command_id == TOGGLE_PIN) {
    return controller_->CanPin();
  } else if (command_id == OPTIONS) {
    const Extension* extension = GetExtension();
    return IsExtensionEnabled(profile_, extension_id_) && extension &&
        !extension->options_url().is_empty();
  } else if (command_id == UNINSTALL) {
    const Extension* extension = GetExtension();
    const extensions::ManagementPolicy* policy =
        extensions::ExtensionSystem::Get(profile_)->management_policy();
    return extension &&
           policy->UserMayModifySettings(extension, NULL);
  } else if (command_id == DETAILS) {
    const Extension* extension = GetExtension();
    return extension && extension->from_webstore();
  }
  return true;
}

bool ExtensionAppItem::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* acclelrator) {
  return false;
}

void ExtensionAppItem::ExecuteCommand(int command_id) {
  if (command_id == LAUNCH) {
    Activate(0);
  } else if (command_id == TOGGLE_PIN && controller_->CanPin()) {
    if (controller_->IsAppPinned(extension_id_))
      controller_->UnpinApp(extension_id_);
    else
      controller_->PinApp(extension_id_);
  } else if (command_id >= LAUNCH_TYPE_START &&
             command_id < LAUNCH_TYPE_LAST) {
    SetExtensionLaunchType(profile_,
                           extension_id_,
                           static_cast<extensions::ExtensionPrefs::LaunchType>(
                               command_id - LAUNCH_TYPE_START));
  } else if (command_id == OPTIONS) {
    ShowExtensionOptions();
  } else if (command_id == UNINSTALL) {
    StartExtensionUninstall();
  } else if (command_id == DETAILS) {
    ShowExtensionDetails();
  }
}

void ExtensionAppItem::Activate(int event_flags) {
  const Extension* extension = GetExtension();
  if (!extension)
    return;

#if defined(OS_CHROMEOS)
  // Talk extension isn't an app, send special rtcPrivate API message to
  // activate it.
  if (IsTalkExtension()) {
    extensions::RtcPrivateEventRouter::DispatchLaunchEvent(
        profile_,
        extensions::RtcPrivateEventRouter::LAUNCH_ACTIVATE,
        NULL /*contact*/);
    return;
  }
#endif  // OS_CHROMEOS

  controller_->ActivateApp(profile_, extension->id(), event_flags);
}

ui::MenuModel* ExtensionAppItem::GetContextMenuModel() {
  // No context menu for Chrome app.
  if (extension_id_ == extension_misc::kChromeAppId)
    return NULL;

  if (!context_menu_model_.get()) {
    context_menu_model_.reset(new ui::SimpleMenuModel(this));
    context_menu_model_->AddItem(LAUNCH, UTF8ToUTF16(title()));
    // Talk extension isn't an app and so doesn't support most launch options.
    if (!IsTalkExtension()) {
      context_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
      context_menu_model_->AddItemWithStringId(
          TOGGLE_PIN,
          controller_->IsAppPinned(extension_id_) ?
              IDS_APP_LIST_CONTEXT_MENU_UNPIN :
              IDS_APP_LIST_CONTEXT_MENU_PIN);
      context_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
      context_menu_model_->AddCheckItemWithStringId(
          LAUNCH_TYPE_REGULAR_TAB,
          IDS_APP_CONTEXT_MENU_OPEN_REGULAR);
      context_menu_model_->AddCheckItemWithStringId(
          LAUNCH_TYPE_PINNED_TAB,
          IDS_APP_CONTEXT_MENU_OPEN_PINNED);
      context_menu_model_->AddCheckItemWithStringId(
          LAUNCH_TYPE_WINDOW,
          IDS_APP_CONTEXT_MENU_OPEN_WINDOW);
      // Even though the launch type is Full Screen it is more accurately
      // described as Maximized in Ash.
      context_menu_model_->AddCheckItemWithStringId(
          LAUNCH_TYPE_FULLSCREEN,
          IDS_APP_CONTEXT_MENU_OPEN_MAXIMIZED);
    }
    context_menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    context_menu_model_->AddItemWithStringId(OPTIONS, IDS_NEW_TAB_APP_OPTIONS);
    context_menu_model_->AddItemWithStringId(DETAILS, IDS_NEW_TAB_APP_DETAILS);
    context_menu_model_->AddItemWithStringId(UNINSTALL,
                                             IDS_EXTENSIONS_UNINSTALL);
  }

  return context_menu_model_.get();
}
