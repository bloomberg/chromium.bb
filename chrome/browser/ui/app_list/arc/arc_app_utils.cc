// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"

#include <memory>
#include <string>

#include "ash/shell.h"
#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/arc_migration_guide_notification.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/ash/launcher/arc_app_deferred_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/arc_app_shelf_id.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/common/intent_helper.mojom.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"

// Helper macro which returns the AppInstance.
#define GET_APP_INSTANCE(method_name)                                    \
  (arc::ArcServiceManager::Get()                                         \
       ? ARC_GET_INSTANCE_FOR_METHOD(                                    \
             arc::ArcServiceManager::Get()->arc_bridge_service()->app(), \
             method_name)                                                \
       : nullptr)

// Helper function which returns the IntentHelperInstance.
#define GET_INTENT_HELPER_INSTANCE(method_name)                    \
  (arc::ArcServiceManager::Get()                                   \
       ? ARC_GET_INSTANCE_FOR_METHOD(arc::ArcServiceManager::Get() \
                                         ->arc_bridge_service()    \
                                         ->intent_helper(),        \
                                     method_name)                  \
       : nullptr)

namespace arc {

namespace {

// Default sizes to use.
constexpr int kNexus7Width = 960;
constexpr int kNexus7Height = 600;
constexpr int kNexus5Width = 410;
constexpr int kNexus5Height = 690;

// Intent helper strings.
constexpr char kIntentHelperClassName[] =
    "org.chromium.arc.intent_helper.SettingsReceiver";
constexpr char kIntentHelperPackageName[] = "org.chromium.arc.intent_helper";
constexpr char kSetInTouchModeIntent[] =
    "org.chromium.arc.intent_helper.SET_IN_TOUCH_MODE";
constexpr char kShowTalkbackSettingsIntent[] =
    "org.chromium.arc.intent_helper.SHOW_TALKBACK_SETTINGS";

constexpr char kAction[] = "action";
constexpr char kActionMain[] = "android.intent.action.MAIN";
constexpr char kCategory[] = "category";
constexpr char kCategoryLauncher[] = "android.intent.category.LAUNCHER";
constexpr char kComponent[] = "component";
constexpr char kEndSuffix[] = "end";
constexpr char kIntentPrefix[] = "#Intent";
constexpr char kLaunchFlags[] = "launchFlags";

constexpr char kAndroidClockAppId[] = "ddmmnabaeomoacfpfjgghfpocfolhjlg";

constexpr char const* kAppIdsHiddenInLauncher[] = {
    kAndroidClockAppId, kSettingsAppId,
};

// Find a proper size and position for a given rectangle on the screen.
// TODO(skuhne): This needs more consideration, but it is lacking
// WindowPositioner functionality since we do not have an Aura::Window yet.
gfx::Rect GetTargetRect(const gfx::Size& size) {
  // Make sure that the window will fit into our workspace.
  // Note that ARC will always be on the primary screen (for now).
  // Note that Android's coordinate system is only valid inside the working
  // area. We can therefore ignore the provided left / top offsets.
  aura::Window* root = ash::Shell::GetPrimaryRootWindow();
  gfx::Rect work_area =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root).work_area();

  gfx::Rect result(size);

  // We do not adjust sizes to fit the window into the work area here.
  // The reason is that the used DP<->pix transformation is different on
  // ChromeOS and ARC dependent on the device and zoom factor being used.
  // As such the real size cannot be determined here (yet). However - ARC
  // will adjust the bounds to not exceed the visible screen later.

  // ChromeOS does not give us much logic at this time, so we emulate what
  // WindowPositioner::GetDefaultWindowBounds does for now.
  // Note: Android's positioning will not overlap the shelf - as such we can
  // ignore the given workspace inset.
  // TODO(skuhne): Replace this with some more useful logic from the
  // WindowPositioner.
  result.set_x((work_area.width() - result.width()) / 2);
  result.set_y((work_area.height() - result.height()) / 2);
  return result;
}

// Returns true if |event_flags| came from a mouse or touch event.
bool IsMouseOrTouchEventFromFlags(int event_flags) {
  return (event_flags & (ui::EF_LEFT_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON |
                         ui::EF_RIGHT_MOUSE_BUTTON | ui::EF_BACK_MOUSE_BUTTON |
                         ui::EF_FORWARD_MOUSE_BUTTON | ui::EF_FROM_TOUCH)) != 0;
}

bool LaunchAppWithRect(content::BrowserContext* context,
                       const std::string& app_id,
                       const base::Optional<std::string>& launch_intent,
                       const gfx::Rect& target_rect,
                       int event_flags) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context);
  CHECK(prefs);

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
  if (!app_info) {
    VLOG(2) << "Cannot launch unavailable app: " << app_id << ".";
    return false;
  }

  if (!app_info->ready) {
    VLOG(2) << "Cannot launch not-ready app: " << app_id << ".";
    return false;
  }

  if (!app_info->launchable) {
    VLOG(2) << "Cannot launch non-launchable app: " << app_id << ".";
    return false;
  }

  arc::mojom::AppInstance* app_instance = GET_APP_INSTANCE(LaunchApp);
  if (!app_instance)
    return false;

  arc::mojom::IntentHelperInstance* intent_helper_instance =
      GET_INTENT_HELPER_INSTANCE(SendBroadcast);
  if (intent_helper_instance) {
    base::DictionaryValue extras;
    extras.SetBoolean("inTouchMode", IsMouseOrTouchEventFromFlags(event_flags));
    std::string extras_string;
    base::JSONWriter::Write(extras, &extras_string);
    intent_helper_instance->SendBroadcast(
        kSetInTouchModeIntent, kIntentHelperPackageName, kIntentHelperClassName,
        extras_string);
  }

  if (app_info->shortcut || launch_intent.has_value()) {
    // Before calling LaunchIntent, check if the interface is supported. Reusing
    // the same |app_instance| for LaunchIntent is allowed.
    if (!GET_APP_INSTANCE(LaunchIntent))
      return false;
    app_instance->LaunchIntent(launch_intent.value_or(app_info->intent_uri),
                               target_rect);
  } else {
    app_instance->LaunchApp(app_info->package_name, app_info->activity,
                            target_rect);
  }
  prefs->SetLastLaunchTime(app_id);

  return true;
}

// A class which handles the asynchronous ARC runtime callback to figure out if
// an app can handle a certain resolution or not.
// After LaunchAndRelease() is called, the object will destroy itself once done.
class AppLauncher {
 public:
  AppLauncher(content::BrowserContext* context,
              const std::string& app_id,
              const base::Optional<std::string>& launch_intent,
              bool landscape_mode,
              int event_flags)
      : context_(context),
        app_id_(app_id),
        launch_intent_(launch_intent),
        landscape_mode_(landscape_mode),
        event_flags_(event_flags) {}

  // This will launch the request and after the return the creator does not
  // need to delete the object anymore.
  bool LaunchAndRelease() {
    landscape_ = landscape_mode_ ? gfx::Rect(0, 0, kNexus7Width, kNexus7Height)
                                 : gfx::Rect(0, 0, kNexus5Width, kNexus5Height);
    if (!ash::Shell::HasInstance()) {
      // Skip this if there is no Ash shell.
      LaunchAppWithRect(context_, app_id_, launch_intent_, landscape_,
                        event_flags_);
      delete this;
      return true;
    }

    // TODO(skuhne): Change CanHandleResolution into a call which returns
    // capability flags like [PHONE/TABLET]_[LANDSCAPE/PORTRAIT] and which
    // might also return the used DP->PIX conversion constant to do better
    // size calculations. base::Unretained is safe because this object is
    // responsible for its own deletion.
    bool result = CanHandleResolution(
        context_, app_id_, landscape_,
        base::Bind(&AppLauncher::Callback, base::Unretained(this)));
    if (!result)
      delete this;

    return result;
  }

 private:
  content::BrowserContext* const context_;
  const std::string app_id_;
  const base::Optional<std::string> launch_intent_;
  const bool landscape_mode_;
  gfx::Rect landscape_;
  const int event_flags_;

  // The callback handler which gets called from the CanHandleResolution
  // function.
  void Callback(bool can_handle) {
    gfx::Size target_size =
        can_handle ? landscape_.size() : gfx::Size(kNexus5Width, kNexus5Height);
    LaunchAppWithRect(context_, app_id_, launch_intent_,
                      GetTargetRect(target_size), event_flags_);
    // Now that we are done, we can delete ourselves.
    delete this;
  }

  DISALLOW_COPY_AND_ASSIGN(AppLauncher);
};

}  // namespace

const char kPlayStoreAppId[] = "cnbgggchhmkkdmeppjobngjoejnihlei";
const char kLegacyPlayStoreAppId[] = "gpkmicpkkebkmabiaedjognfppcchdfa";
const char kPlayStorePackage[] = "com.android.vending";
const char kPlayStoreActivity[] = "com.android.vending.AssetBrowserActivity";
const char kSettingsAppId[] = "mconboelelhjpkbdhhiijkgcimoangdj";
const char kInitialStartParam[] = "S.org.chromium.arc.start_type=initialStart";

bool ShouldShowInLauncher(const std::string& app_id) {
  for (auto* const id : kAppIdsHiddenInLauncher) {
    if (id == app_id)
      return false;
  }
  return true;
}

bool LaunchAndroidSettingsApp(content::BrowserContext* context,
                              int event_flags) {
  constexpr bool kUseLandscapeLayout = true;
  return arc::LaunchApp(context, kSettingsAppId, kUseLandscapeLayout,
                        event_flags);
}

bool LaunchPlayStoreWithUrl(const std::string& url) {
  arc::mojom::IntentHelperInstance* instance =
      GET_INTENT_HELPER_INSTANCE(HandleUrl);
  if (!instance) {
    VLOG(1) << "Cannot find a mojo instance, ARC is unreachable";
    return false;
  }
  instance->HandleUrl(url, kPlayStorePackage);
  return true;
}

bool LaunchApp(content::BrowserContext* context,
               const std::string& app_id,
               int event_flags) {
  constexpr bool kUseLandscapeLayout = true;
  return LaunchApp(context, app_id, kUseLandscapeLayout, event_flags);
}

bool LaunchApp(content::BrowserContext* context,
               const std::string& app_id,
               bool landscape_layout,
               int event_flags) {
  return LaunchAppWithIntent(context, app_id,
                             base::Optional<std::string>() /* launch_intent */,
                             landscape_layout, event_flags);
}

bool LaunchAppWithIntent(content::BrowserContext* context,
                         const std::string& app_id,
                         const base::Optional<std::string>& launch_intent,
                         bool landscape_layout,
                         int event_flags) {
  DCHECK(!launch_intent.has_value() || !launch_intent->empty());

  Profile* const profile = Profile::FromBrowserContext(context);

  // Even when ARC is not allowed for the profile, ARC apps may still show up
  // as a placeholder to show the guide notification for proper configuration.
  // Handle such a case here and shows the desired notification.
  if (IsArcBlockedDueToIncompatibleFileSystem(profile)) {
    arc::ShowArcMigrationGuideNotification(profile);
    return false;
  }

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
  if (app_info && !app_info->ready) {
    if (!IsArcPlayStoreEnabledForProfile(profile)) {
      if (prefs->IsDefault(app_id)) {
        // The setting can fail if the preference is managed.  However, the
        // caller is responsible to not call this function in such case.  DCHECK
        // is here to prevent possible mistake.
        SetArcPlayStoreEnabledForProfile(profile, true);
        DCHECK(IsArcPlayStoreEnabledForProfile(profile));

        // PlayStore item has special handling for shelf controllers. In order
        // to avoid unwanted initial animation for PlayStore item do not create
        // deferred launch request when PlayStore item enables Google Play
        // Store.
        if (app_id == kPlayStoreAppId) {
          prefs->SetLastLaunchTime(app_id);
          return true;
        }
      } else {
        // Only reachable when ARC always starts.
        DCHECK(arc::ShouldArcAlwaysStart());
      }
    } else {
      // Handle the case when default app tries to re-activate OptIn flow.
      if (IsArcPlayStoreEnabledPreferenceManagedForProfile(profile) &&
          !ArcSessionManager::Get()->enable_requested() &&
          prefs->IsDefault(app_id)) {
        SetArcPlayStoreEnabledForProfile(profile, true);
        // PlayStore item has special handling for shelf controllers. In order
        // to avoid unwanted initial animation for PlayStore item do not create
        // deferred launch request when PlayStore item enables Google Play
        // Store.
        if (app_id == kPlayStoreAppId) {
          prefs->SetLastLaunchTime(app_id);
          return true;
        }
      }
    }

    ChromeLauncherController* chrome_controller =
        ChromeLauncherController::instance();
    DCHECK(chrome_controller || !ash::Shell::HasInstance());
    if (chrome_controller) {
      chrome_controller->GetArcDeferredLauncher()->RegisterDeferredLaunch(
          app_id, event_flags);

      // On some boards, ARC is booted with a restricted set of resources by
      // default to avoid slowing down Chrome's user session restoration.
      // However, the restriction should be lifted once the user explicitly
      // tries to launch an ARC app.
      SetArcCpuRestriction(false);
    }
    prefs->SetLastLaunchTime(app_id);
    return true;
  }
  return (new AppLauncher(context, app_id, launch_intent, landscape_layout,
                          event_flags))
      ->LaunchAndRelease();
}

void SetTaskActive(int task_id) {
  arc::mojom::AppInstance* app_instance = GET_APP_INSTANCE(SetTaskActive);
  if (!app_instance)
    return;
  app_instance->SetTaskActive(task_id);
}

void CloseTask(int task_id) {
  arc::mojom::AppInstance* app_instance = GET_APP_INSTANCE(CloseTask);
  if (!app_instance)
    return;
  app_instance->CloseTask(task_id);
}

void ShowTalkBackSettings() {
  arc::mojom::IntentHelperInstance* intent_helper_instance =
      GET_INTENT_HELPER_INSTANCE(SendBroadcast);
  if (!intent_helper_instance)
    return;

  intent_helper_instance->SendBroadcast(kShowTalkbackSettingsIntent,
                                        kIntentHelperPackageName,
                                        kIntentHelperClassName, "{}");
}

void StartPaiFlow() {
  arc::mojom::AppInstance* app_instance = GET_APP_INSTANCE(StartPaiFlow);
  if (!app_instance)
    return;
  app_instance->StartPaiFlow();
}

bool CanHandleResolution(content::BrowserContext* context,
                         const std::string& app_id,
                         const gfx::Rect& rect,
                         const CanHandleResolutionCallback& callback) {
  const ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context);
  DCHECK(prefs);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
  if (!app_info) {
    VLOG(2) << "Cannot test resolution capability of unavailable app:" << app_id
            << ".";
    return false;
  }

  arc::mojom::AppInstance* app_instance = GET_APP_INSTANCE(CanHandleResolution);
  if (!app_instance)
    return false;

  app_instance->CanHandleResolution(app_info->package_name, app_info->activity,
                                    rect, callback);
  return true;
}

void UninstallPackage(const std::string& package_name) {
  VLOG(2) << "Uninstalling " << package_name;

  arc::mojom::AppInstance* app_instance = GET_APP_INSTANCE(UninstallPackage);
  if (!app_instance)
    return;

  app_instance->UninstallPackage(package_name);
}

void UninstallArcApp(const std::string& app_id, Profile* profile) {
  ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile);
  DCHECK(arc_prefs);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_prefs->GetApp(app_id);
  if (!app_info) {
    VLOG(2) << "Package being uninstalled does not exist: " << app_id << ".";
    return;
  }
  // For shortcut we just remove the shortcut instead of the package.
  if (app_info->shortcut)
    arc_prefs->RemoveApp(app_id);
  else
    UninstallPackage(app_info->package_name);
}

void RemoveCachedIcon(const std::string& icon_resource_id) {
  VLOG(2) << "Removing icon " << icon_resource_id;

  arc::mojom::AppInstance* app_instance = GET_APP_INSTANCE(RemoveCachedIcon);
  if (!app_instance)
    return;

  app_instance->RemoveCachedIcon(icon_resource_id);
}

// Deprecated.
bool ShowPackageInfo(const std::string& package_name) {
  VLOG(2) << "Showing package info for " << package_name;

  arc::mojom::AppInstance* app_instance =
      GET_APP_INSTANCE(ShowPackageInfoDeprecated);
  if (!app_instance)
    return false;

  app_instance->ShowPackageInfoDeprecated(
      package_name, GetTargetRect(gfx::Size(kNexus7Width, kNexus7Height)));
  return true;
}

bool ShowPackageInfoOnPage(const std::string& package_name,
                           mojom::ShowPackageInfoPage page) {
  VLOG(2) << "Showing package info for " << package_name;

  arc::mojom::AppInstance* app_instance =
      GET_APP_INSTANCE(ShowPackageInfoOnPage);
  if (!app_instance)
    return false;

  app_instance->ShowPackageInfoOnPage(
      package_name, page,
      GetTargetRect(gfx::Size(kNexus7Width, kNexus7Height)));
  return true;
}

bool IsArcItem(content::BrowserContext* context, const std::string& id) {
  DCHECK(context);

  // Some unit tests use empty id.
  if (id.empty())
    return false;

  const ArcAppListPrefs* const arc_prefs = ArcAppListPrefs::Get(context);
  if (!arc_prefs)
    return false;

  return arc_prefs->IsRegistered(ArcAppShelfId::FromString(id).app_id());
}

std::string GetLaunchIntent(const std::string& package_name,
                            const std::string& activity,
                            const std::vector<std::string>& extra_params) {
  std::string extra_params_extracted;
  for (const auto& extra_param : extra_params) {
    extra_params_extracted += extra_param;
    extra_params_extracted += ";";
  }

  // Remove the |package_name| prefix, if activity starts with it.
  const char* activity_compact_name =
      activity.find(package_name.c_str()) == 0
          ? activity.c_str() + package_name.length()
          : activity.c_str();

  // Construct a string in format:
  // #Intent;action=android.intent.action.MAIN;
  //         category=android.intent.category.LAUNCHER;
  //         launchFlags=0x10210000;
  //         component=package_name/activity;
  //         param1;param2;end
  return base::StringPrintf(
      "%s;%s=%s;%s=%s;%s=0x%x;%s=%s/%s;%s%s", kIntentPrefix, kAction,
      kActionMain, kCategory, kCategoryLauncher, kLaunchFlags,
      Intent::FLAG_ACTIVITY_NEW_TASK |
          Intent::FLAG_ACTIVITY_RESET_TASK_IF_NEEDED,
      kComponent, package_name.c_str(), activity_compact_name,
      extra_params_extracted.c_str(), kEndSuffix);
}

bool ParseIntent(const std::string& intent_as_string, Intent* intent) {
  DCHECK(intent);
  const std::vector<base::StringPiece> parts = base::SplitStringPiece(
      intent_as_string, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (parts.size() < 2 || parts.front() != kIntentPrefix ||
      parts.back() != kEndSuffix) {
    DVLOG(1) << "Failed to split intent " << intent_as_string << ".";
    return false;
  }

  for (size_t i = 1; i < parts.size() - 1; ++i) {
    const size_t separator = parts[i].find('=');
    if (separator == std::string::npos) {
      intent->AddExtraParam(parts[i].as_string());
      continue;
    }
    const base::StringPiece key = parts[i].substr(0, separator);
    const base::StringPiece value = parts[i].substr(separator + 1);
    if (key == kAction) {
      intent->set_action(value.as_string());
    } else if (key == kCategory) {
      intent->set_category(value.as_string());
    } else if (key == kLaunchFlags) {
      uint32_t launch_flags;
      const bool parsed =
          base::HexStringToUInt(value.as_string(), &launch_flags);
      if (!parsed) {
        DVLOG(1) << "Failed to parse launchFlags: " << value.as_string() << ".";
        return false;
      }
      intent->set_launch_flags(launch_flags);
    } else if (key == kComponent) {
      const size_t component_separator = value.find('/');
      if (component_separator == std::string::npos)
        return false;
      intent->set_package_name(
          value.substr(0, component_separator).as_string());
      const base::StringPiece activity_compact_name =
          value.substr(component_separator + 1);
      if (!activity_compact_name.empty() && activity_compact_name[0] == '.') {
        std::string activity = value.substr(0, component_separator).as_string();
        activity += activity_compact_name.as_string();
        intent->set_activity(activity);
      } else {
        intent->set_activity(activity_compact_name.as_string());
      }
    } else {
      intent->AddExtraParam(parts[i].as_string());
    }
  }

  return true;
}

Intent::Intent() = default;

Intent::~Intent() = default;

void Intent::AddExtraParam(const std::string& extra_param) {
  extra_params_.push_back(extra_param);
}

bool Intent::HasExtraParam(const std::string& extra_param) const {
  return std::find(extra_params_.begin(), extra_params_.end(), extra_param) !=
         extra_params_.end();
}

}  // namespace arc
