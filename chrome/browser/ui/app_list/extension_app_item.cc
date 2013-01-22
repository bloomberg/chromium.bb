// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/extension_app_item.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_sorting.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/management_policy.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "content/public/common/context_menu_params.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"

using extensions::Extension;

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

// ExtensionUninstaller decouples ExtensionAppItem from the extension uninstall
// flow. It shows extension uninstall dialog and wait for user to confirm or
// cancel the uninstall.
class ExtensionUninstaller : public ExtensionUninstallDialog::Delegate {
 public:
  ExtensionUninstaller(Profile* profile,
                       const std::string& extension_id,
                       AppListControllerDelegate* controller)
      : profile_(profile),
        extension_id_(extension_id),
        controller_(controller) {
  }

  void Run() {
    const Extension* extension =
        extensions::ExtensionSystem::Get(profile_)->extension_service()->
            GetExtensionById(extension_id_, true);
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
    const Extension* extension = service->GetInstalledExtension(extension_id_);
    if (extension) {
      service->UninstallExtension(extension_id_,
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
  std::string extension_id_;
  AppListControllerDelegate* controller_;
  scoped_ptr<ExtensionUninstallDialog> dialog_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUninstaller);
};

class TabOverlayImageSource : public gfx::CanvasImageSource {
 public:
  TabOverlayImageSource(const gfx::ImageSkia& icon, const gfx::Size& size)
      : gfx::CanvasImageSource(size, false),
        icon_(icon) {
    DCHECK_EQ(extension_misc::EXTENSION_ICON_SMALL, icon_.width());
    DCHECK_EQ(extension_misc::EXTENSION_ICON_SMALL, icon_.height());
  }
  virtual ~TabOverlayImageSource() {}

 private:
  // gfx::CanvasImageSource overrides:
  void Draw(gfx::Canvas* canvas) OVERRIDE {
    using extension_misc::EXTENSION_ICON_SMALL;
    using extension_misc::EXTENSION_ICON_MEDIUM;

    const int kIconOffset = (EXTENSION_ICON_MEDIUM - EXTENSION_ICON_SMALL) / 2;

    // The tab overlay is not vertically symmetric, to position the app in the
    // middle of the overlay we need a slight adjustment.
    const int kVerticalAdjust = 4;
    canvas->DrawImageInt(
        *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_APP_LIST_TAB_OVERLAY),
        0, 0);
    canvas->DrawImageInt(icon_, kIconOffset, kIconOffset + kVerticalAdjust);
  }

  gfx::ImageSkia icon_;

  DISALLOW_COPY_AND_ASSIGN(TabOverlayImageSource);
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

ExtensionSorting* GetExtensionSorting(Profile* profile) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  return service->extension_prefs()->extension_sorting();
}

bool MenuItemHasLauncherContext(const extensions::MenuItem* item) {
  return item->contexts().Contains(extensions::MenuItem::LAUNCHER);
}

bool HasOverlay(const Extension* extension) {
#if defined(OS_CHROMEOS)
  return false;
#else
  return !extension->is_platform_app();
#endif
}

}  // namespace

ExtensionAppItem::ExtensionAppItem(Profile* profile,
                                   const Extension* extension,
                                   AppListControllerDelegate* controller)
    : ChromeAppListItem(TYPE_APP),
      profile_(profile),
      extension_id_(extension->id()),
      controller_(controller),
      has_overlay_(HasOverlay(extension)) {
  Reload();
}

ExtensionAppItem::~ExtensionAppItem() {
}

void ExtensionAppItem::Reload() {
  const Extension* extension = GetExtension();
  SetTitle(extension->name());
  LoadImage(extension);
}

syncer::StringOrdinal ExtensionAppItem::GetPageOrdinal() const {
  return GetExtensionSorting(profile_)->GetPageOrdinal(extension_id_);
}

syncer::StringOrdinal ExtensionAppItem::GetAppLaunchOrdinal() const {
  return GetExtensionSorting(profile_)->GetAppLaunchOrdinal(extension_id_);
}

void ExtensionAppItem::Move(const ExtensionAppItem* prev,
                            const ExtensionAppItem* next) {
  // Does nothing if no predecessor nor successor.
  if (!prev && !next)
    return;

  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  service->extension_prefs()->SetAppDraggedByUser(extension_id_);

  // Handles only predecessor or only successor case.
  if (!prev || !next) {
    syncer::StringOrdinal page = prev ? prev->GetPageOrdinal() :
                                        next->GetPageOrdinal();
    GetExtensionSorting(profile_)->SetPageOrdinal(extension_id_, page);
    service->OnExtensionMoved(extension_id_,
                              prev ? prev->extension_id() : std::string(),
                              next ? next->extension_id() : std::string());
    return;
  }

  // Handles both predecessor and successor are on the same page.
  syncer::StringOrdinal prev_page = prev->GetPageOrdinal();
  syncer::StringOrdinal next_page = next->GetPageOrdinal();
  if (prev_page.Equals(next_page)) {
    GetExtensionSorting(profile_)->SetPageOrdinal(extension_id_, prev_page);
    service->OnExtensionMoved(extension_id_,
                              prev->extension_id(),
                              next->extension_id());
    return;
  }

  // Otherwise, go with |next|. This is okay because app list does not split
  // page based ntp page ordinal.
  // TODO(xiyuan): Revisit this when implementing paging support.
  GetExtensionSorting(profile_)->SetPageOrdinal(extension_id_, prev_page);
  service->OnExtensionMoved(extension_id_,
                            prev->extension_id(),
                            std::string());
}

void ExtensionAppItem::UpdateIcon() {
  gfx::ImageSkia icon = icon_->image_skia();

  const ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  const bool enabled = service->IsExtensionEnabledForLauncher(extension_id_);
  if (!enabled) {
    const color_utils::HSL shift = {-1, 0, 0.6};
    icon = gfx::ImageSkiaOperations::CreateHSLShiftedImage(icon, shift);
  }

  if (has_overlay_) {
    const gfx::Size size(extension_misc::EXTENSION_ICON_MEDIUM,
                         extension_misc::EXTENSION_ICON_MEDIUM);
    icon = gfx::ImageSkia(new TabOverlayImageSource(icon, size), size);
  }

  SetIcon(icon);
}

const Extension* ExtensionAppItem::GetExtension() const {
  const ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  const Extension* extension = service->GetInstalledExtension(extension_id_);
  return extension;
}

void ExtensionAppItem::LoadImage(const Extension* extension) {
  int icon_size = extension_misc::EXTENSION_ICON_MEDIUM;
  if (has_overlay_)
    icon_size = extension_misc::EXTENSION_ICON_SMALL;

  icon_.reset(new extensions::IconImage(
      extension,
      extension->icons(),
      icon_size,
      Extension::GetDefaultIcon(true),
      this));
  UpdateIcon();
}

void ExtensionAppItem::ShowExtensionOptions() {
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  chrome::NavigateParams params(
      profile_,
      extensions::ManifestURL::GetOptionsPage(extension),
      content::PAGE_TRANSITION_LINK);
  chrome::Navigate(&params);
}

void ExtensionAppItem::ShowExtensionDetails() {
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  chrome::NavigateParams params(
      profile_,
      extensions::ManifestURL::GetDetailsURL(extension),
      content::PAGE_TRANSITION_LINK);
  chrome::Navigate(&params);
}

void ExtensionAppItem::StartExtensionUninstall() {
  // ExtensionUninstall deletes itself when done or aborted.
  ExtensionUninstaller* uninstaller = new ExtensionUninstaller(profile_,
                                                               extension_id_,
                                                               controller_);
  uninstaller->Run();
}

bool ExtensionAppItem::RunExtensionEnableFlow() {
  const ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  if (service->IsExtensionEnabledForLauncher(extension_id_))
    return false;

  if (!extension_enable_flow_) {
    controller_->OnShowExtensionPrompt();

    extension_enable_flow_.reset(new ExtensionEnableFlow(
        profile_, extension_id_, this));
    extension_enable_flow_->StartForNativeWindow(
        controller_->GetAppListWindow());
  }
  return true;
}

void ExtensionAppItem::Launch(int event_flags) {
  // |extension| could be NULL when it is being unloaded for updating.
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  if (RunExtensionEnableFlow())
    return;

  controller_->LaunchApp(profile_, extension, event_flags);
}

void ExtensionAppItem::OnExtensionIconImageChanged(
    extensions::IconImage* image) {
  DCHECK(icon_.get() == image);
  UpdateIcon();
}

void ExtensionAppItem::ExtensionEnableFlowFinished() {
  extension_enable_flow_.reset();
  controller_->OnCloseExtensionPrompt();

  // Automatically launch app after enabling.
  Launch(ui::EF_NONE);
}

void ExtensionAppItem::ExtensionEnableFlowAborted(bool user_initiated) {
  extension_enable_flow_.reset();
  controller_->OnCloseExtensionPrompt();
}

bool ExtensionAppItem::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == TOGGLE_PIN || command_id == LAUNCH_NEW;
}

string16 ExtensionAppItem::GetLabelForCommandId(int command_id) const {
  if (command_id == TOGGLE_PIN) {
    return controller_->IsAppPinned(extension_id_) ?
        l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_UNPIN) :
        l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_PIN);
  } else if (command_id == LAUNCH_NEW) {
    if (IsCommandIdChecked(LAUNCH_TYPE_PINNED_TAB) ||
        IsCommandIdChecked(LAUNCH_TYPE_REGULAR_TAB)) {
      return l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_NEW_TAB);
    } else {
      return l10n_util::GetStringUTF16(IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW);
    }
  } else {
    NOTREACHED();
    return string16();
  }
}

bool ExtensionAppItem::IsCommandIdChecked(int command_id) const {
  if (command_id >= LAUNCH_TYPE_START && command_id < LAUNCH_TYPE_LAST) {
    return static_cast<int>(GetExtensionLaunchType(profile_, GetExtension())) +
        LAUNCH_TYPE_START == command_id;
  } else if (command_id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
             command_id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST) {
    return extension_menu_items_->IsCommandIdChecked(command_id);
  }
  return false;
}

bool ExtensionAppItem::IsCommandIdEnabled(int command_id) const {
  if (command_id == TOGGLE_PIN) {
    return controller_->CanPin();
  } else if (command_id == OPTIONS) {
    const ExtensionService* service =
        extensions::ExtensionSystem::Get(profile_)->extension_service();
    const Extension* extension = GetExtension();
    return service->IsExtensionEnabledForLauncher(extension_id_) &&
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

bool ExtensionAppItem::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* acclelrator) {
  return false;
}

void ExtensionAppItem::ExecuteCommand(int command_id) {
  if (command_id == LAUNCH_NEW) {
    Launch(ui::EF_NONE);
  } else if (command_id == TOGGLE_PIN && controller_->CanPin()) {
    if (controller_->IsAppPinned(extension_id_))
      controller_->UnpinApp(extension_id_);
    else
      controller_->PinApp(extension_id_);
  } else if (command_id == CREATE_SHORTCUTS) {
    controller_->ShowCreateShortcutsDialog(profile_, extension_id_);
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
  } else if (command_id >= IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
             command_id <= IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST) {
    extension_menu_items_->ExecuteCommand(command_id, NULL,
                                          content::ContextMenuParams());
  } else if (command_id == MENU_NEW_WINDOW) {
    controller_->CreateNewWindow(false);
  } else if (command_id == MENU_NEW_INCOGNITO_WINDOW) {
    controller_->CreateNewWindow(true);
  }
}

void ExtensionAppItem::Activate(int event_flags) {
  // |extension| could be NULL when it is being unloaded for updating.
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  if (RunExtensionEnableFlow())
    return;

  controller_->ActivateApp(profile_, extension, event_flags);
}

ui::MenuModel* ExtensionAppItem::GetContextMenuModel() {
  const Extension* extension = GetExtension();
  if (!extension)
    return NULL;

  if (context_menu_model_.get())
    return context_menu_model_.get();

  context_menu_model_.reset(new ui::SimpleMenuModel(this));

  if (extension_id_ == extension_misc::kChromeAppId) {
    // Special context menu for Chrome app.
#if defined(OS_CHROMEOS)
    context_menu_model_->AddItemWithStringId(
        MENU_NEW_WINDOW,
        IDS_LAUNCHER_NEW_WINDOW);
    if (!profile_->IsOffTheRecord()) {
      context_menu_model_->AddItemWithStringId(
          MENU_NEW_INCOGNITO_WINDOW,
          IDS_LAUNCHER_NEW_INCOGNITO_WINDOW);
    }
#else
    NOTREACHED();
#endif
  } else {
    extension_menu_items_.reset(new extensions::ContextMenuMatcher(
        profile_, this, context_menu_model_.get(),
        base::Bind(MenuItemHasLauncherContext)));

    if (!extension->is_platform_app())
      context_menu_model_->AddItem(LAUNCH_NEW, string16());

    int index = 0;
    extension_menu_items_->AppendExtensionItems(extension_id_, string16(),
                                                &index);

    if (controller_->CanPin()) {
      context_menu_model_->AddSeparatorIfNecessary(ui::NORMAL_SEPARATOR);
      context_menu_model_->AddItemWithStringId(
          TOGGLE_PIN,
          controller_->IsAppPinned(extension_id_) ?
              IDS_APP_LIST_CONTEXT_MENU_UNPIN :
              IDS_APP_LIST_CONTEXT_MENU_PIN);
    }

    if (controller_->CanShowCreateShortcutsDialog() &&
        extension->is_platform_app()) {
      context_menu_model_->AddItemWithStringId(
          CREATE_SHORTCUTS,
          IDS_NEW_TAB_APP_CREATE_SHORTCUT);
    }

    if (!extension->is_platform_app()) {
      context_menu_model_->AddSeparatorIfNecessary(ui::NORMAL_SEPARATOR);
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

    if (!extension->is_platform_app()) {
      context_menu_model_->AddSeparatorIfNecessary(ui::NORMAL_SEPARATOR);
      context_menu_model_->AddItemWithStringId(OPTIONS,
                                               IDS_NEW_TAB_APP_OPTIONS);
      context_menu_model_->AddItemWithStringId(DETAILS,
                                               IDS_NEW_TAB_APP_DETAILS);
    }

    context_menu_model_->AddItemWithStringId(UNINSTALL,
                                             extension->is_platform_app() ?
                                                 IDS_APP_LIST_UNINSTALL_ITEM :
                                                 IDS_EXTENSIONS_UNINSTALL);
  }

  return context_menu_model_.get();
}
