// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/app_list/extension_app_item.h"

#include "ash/app_list/app_list_item_view.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/event_disposition.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/ash/launcher/chrome_launcher_delegate.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_resource.h"
#include "grit/chromium_strings.h"
#include "grit/component_extension_resources_map.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
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
  return ChromeLauncherDelegate::instance()->IsAppPinned(extension_id);
}

void PinApp(const std::string& extension_id,
            ExtensionPrefs::LaunchType launch_type) {
  ChromeLauncherDelegate::AppType app_type =
      ChromeLauncherDelegate::APP_TYPE_TAB;
  switch (launch_type) {
    case ExtensionPrefs::LAUNCH_PINNED:
    case ExtensionPrefs::LAUNCH_REGULAR:
      app_type = ChromeLauncherDelegate::APP_TYPE_TAB;
      break;
    case ExtensionPrefs::LAUNCH_FULLSCREEN:
    case ExtensionPrefs::LAUNCH_WINDOW:
      app_type = ChromeLauncherDelegate::APP_TYPE_WINDOW;
      break;
    default:
      NOTREACHED() << "Unknown launch_type=" << launch_type;
      break;
  }

  ChromeLauncherDelegate::instance()->PinAppWithID(extension_id, app_type);
}

void UnpinApp(const std::string& extension_id) {
  return ChromeLauncherDelegate::instance()->UnpinAppsWithID(extension_id);
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
  ExtensionResource icon = extension->GetIconResource(
      ExtensionIconSet::EXTENSION_ICON_LARGE,
      ExtensionIconSet::MATCH_BIGGER);
  if (icon.relative_path().empty()) {
    LoadDefaultImage();
    return;
  }

  if (extension->location() == Extension::COMPONENT) {
    FilePath directory_path = extension->path();
    FilePath relative_path = directory_path.BaseName().Append(
        icon.relative_path());
    for (size_t i = 0; i < kComponentExtensionResourcesSize; ++i) {
      FilePath bm_resource_path =
          FilePath().AppendASCII(kComponentExtensionResources[i].name);
      bm_resource_path = bm_resource_path.NormalizePathSeparators();
      if (relative_path == bm_resource_path) {
        ResourceBundle& rb = ResourceBundle::GetSharedInstance();
        int resource = kComponentExtensionResources[i].value;

        base::StringPiece contents = rb.GetRawDataResource(resource);
        SkBitmap icon;
        if (gfx::PNGCodec::Decode(
            reinterpret_cast<const unsigned char*>(contents.data()),
            contents.size(), &icon)) {
          SetIcon(icon);
          return;
        } else {
          NOTREACHED() << "Unable to decode image resource " << resource;
        }
      }
    }
  }

  tracker_.reset(new ImageLoadingTracker(this));
  tracker_->LoadImage(extension,
                      icon,
                      gfx::Size(ExtensionIconSet::EXTENSION_ICON_LARGE,
                                ExtensionIconSet::EXTENSION_ICON_LARGE),
                      ImageLoadingTracker::DONT_CACHE);
}

void ExtensionAppItem::LoadDefaultImage() {
  const Extension* extension = GetExtension();
  int resource = IDR_APP_DEFAULT_ICON;
  if (extension && extension->id() == extension_misc::kWebStoreAppId)
      resource = IDR_WEBSTORE_ICON;

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SetIcon(*rb.GetImageNamed(resource).ToSkBitmap());
}

void ExtensionAppItem::ShowExtensionOptions() {
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  if (!browser)
    browser = Browser::Create(profile_);

  if (browser) {
    browser->AddSelectedTabWithURL(extension->options_url(),
                                   content::PAGE_TRANSITION_LINK);
    browser->window()->Activate();
  }
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
    LoadDefaultImage();
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
    if (IsAppPinned(extension_id_)) {
      UnpinApp(extension_id_);
    } else {
      PinApp(extension_id_,
             GetExtensionLaunchType(profile_, extension_id_));
    }
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

  WindowOpenDisposition disposition =
      browser::DispositionFromEventFlags(event_flags);

  GURL url;
  if (extension_id_ == extension_misc::kWebStoreAppId)
    url = extension->GetFullLaunchURL();

  if (disposition == NEW_FOREGROUND_TAB || disposition == NEW_BACKGROUND_TAB) {
    // Opens in a tab.
    Browser::OpenApplication(
        profile_, extension, extension_misc::LAUNCH_TAB, url, disposition);
  } else if (disposition == NEW_WINDOW) {
    // Force a new window open.
    Browser::OpenApplication(
        profile_, extension, extension_misc::LAUNCH_WINDOW, url,
        disposition);
  } else {
    // Look at preference to find the right launch container.  If no preference
    // is set, launch as a regular tab.
    extension_misc::LaunchContainer launch_container =
        profile_->GetExtensionService()->extension_prefs()->GetLaunchContainer(
            extension, ExtensionPrefs::LAUNCH_REGULAR);

    Browser::OpenApplication(
        profile_, extension, launch_container, GURL(url),
        NEW_FOREGROUND_TAB);
  }
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
    context_menu_model_->AddCheckItemWithStringId(
        LAUNCH_TYPE_FULLSCREEN,
        IDS_APP_CONTEXT_MENU_OPEN_FULLSCREEN);
    context_menu_model_->AddSeparator();
    context_menu_model_->AddItemWithStringId(OPTIONS, IDS_NEW_TAB_APP_OPTIONS);
    context_menu_model_->AddItemWithStringId(UNINSTALL,
                                             IDS_EXTENSIONS_UNINSTALL);
  }

  return context_menu_model_.get();
}

