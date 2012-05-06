// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/app_list/extension_app_item.h"

#include "ash/app_list/app_list_item_view.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/ash/extension_utils.h"
#include "chrome/browser/ui/views/ash/launcher/chrome_launcher_controller.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

namespace {

enum CommandId {
  LAUNCH = 100,
  TOGGLE_PIN,
  OPTIONS,
  UNINSTALL,
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
        ExtensionUninstallDialog::Create(profile_, this);
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

ExtensionPrefs::LaunchType GetExtensionLaunchType(
    Profile* profile,
    const std::string& extension_id) {
  return profile->GetExtensionService()->extension_prefs()->GetLaunchType(
      extension_id, ExtensionPrefs::LAUNCH_DEFAULT);
}

void SetExtensionLaunchType(Profile* profile,
                            const std::string& extension_id,
                            ExtensionPrefs::LaunchType launch_type) {
  profile->GetExtensionService()->extension_prefs()->SetLaunchType(
      extension_id, launch_type);
}

bool IsExtensionEnabled(Profile* profile, const std::string& extension_id) {
  ExtensionService* service = profile->GetExtensionService();
  return service->IsExtensionEnabled(extension_id) &&
      !service->GetTerminatedExtension(extension_id);
}

bool IsAppPinned(const std::string& extension_id) {
  return ChromeLauncherController::instance()->IsAppPinned(extension_id);
}

void PinApp(const std::string& extension_id) {
  ChromeLauncherController::instance()->PinAppWithID(extension_id);
}

void UnpinApp(const std::string& extension_id) {
  return ChromeLauncherController::instance()->UnpinAppsWithID(extension_id);
}

}  // namespace

ExtensionAppItem::ExtensionAppItem(Profile* profile,
                                   const Extension* extension)
    : ChromeAppListItem(TYPE_APP),
      profile_(profile),
      extension_id_(extension->id()) {
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

void ExtensionAppItem::LoadImage(const Extension* extension) {
  tracker_.reset(new ImageLoadingTracker(this));
  tracker_->LoadImage(extension,
                      extension->GetIconResource(
                          ExtensionIconSet::EXTENSION_ICON_LARGE,
                          ExtensionIconSet::MATCH_BIGGER),
                      gfx::Size(ExtensionIconSet::EXTENSION_ICON_LARGE,
                                ExtensionIconSet::EXTENSION_ICON_LARGE),
                      ImageLoadingTracker::DONT_CACHE);
}

void ExtensionAppItem::ShowExtensionOptions() {
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  if (!browser) {
    browser = Browser::Create(profile_);
    browser->window()->Show();
  }

  browser->AddSelectedTabWithURL(extension->options_url(),
                                 content::PAGE_TRANSITION_LINK);
  browser->window()->Activate();
}

void ExtensionAppItem::StartExtensionUninstall() {
  // ExtensionUninstall deletes itself when done or aborted.
  ExtensionUninstaller* uninstaller = new ExtensionUninstaller(profile_,
                                                               extension_id_);
  uninstaller->Run();
}

void ExtensionAppItem::OnImageLoaded(const gfx::Image& image,
                                     const std::string& extension_id,
                                     int tracker_index) {
  if (!image.IsEmpty())
    SetIcon(*image.ToSkBitmap());
  else
    SetIcon(Extension::GetDefaultIcon(true /* is_app */));
}

bool ExtensionAppItem::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == TOGGLE_PIN;
}

string16 ExtensionAppItem::GetLabelForCommandId(int command_id) const {
  if (command_id == TOGGLE_PIN) {
    return IsAppPinned(extension_id_) ?
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
  if (command_id == OPTIONS) {
    const Extension* extension = GetExtension();
    return IsExtensionEnabled(profile_, extension_id_) && extension &&
        !extension->options_url().is_empty();
  } else if (command_id == UNINSTALL) {
    const Extension* extension = GetExtension();
    return extension && Extension::UserMayDisable(extension->location());
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
  } else if (command_id == TOGGLE_PIN) {
    if (IsAppPinned(extension_id_))
      UnpinApp(extension_id_);
    else
      PinApp(extension_id_);
  } else if (command_id >= LAUNCH_TYPE_START &&
             command_id < LAUNCH_TYPE_LAST) {
    SetExtensionLaunchType(profile_,
                           extension_id_,
                           static_cast<ExtensionPrefs::LaunchType>(
                               command_id - LAUNCH_TYPE_START));
  } else if (command_id == OPTIONS) {
    ShowExtensionOptions();
  } else if (command_id == UNINSTALL) {
    StartExtensionUninstall();
  }
}

void ExtensionAppItem::Activate(int event_flags) {
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  extension_utils::OpenExtension(profile_, extension, event_flags);
}

ui::MenuModel* ExtensionAppItem::GetContextMenuModel() {
  if (!context_menu_model_.get()) {
    context_menu_model_.reset(new ui::SimpleMenuModel(this));
    context_menu_model_->AddItem(LAUNCH, UTF8ToUTF16(title()));
    context_menu_model_->AddSeparator();
    context_menu_model_->AddItemWithStringId(
        TOGGLE_PIN,
        IsAppPinned(extension_id_) ? IDS_APP_LIST_CONTEXT_MENU_UNPIN :
                                     IDS_APP_LIST_CONTEXT_MENU_PIN);
    context_menu_model_->AddSeparator();
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
    context_menu_model_->AddSeparator();
    context_menu_model_->AddItemWithStringId(OPTIONS, IDS_NEW_TAB_APP_OPTIONS);
    context_menu_model_->AddItemWithStringId(UNINSTALL,
                                             IDS_EXTENSIONS_UNINSTALL);
  }

  return context_menu_model_.get();
}

