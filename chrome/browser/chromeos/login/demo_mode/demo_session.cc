// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/sys_info.h"
#include "chrome/browser/apps/platform_apps/app_load_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/settings/install_attributes.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/image_loader_client.h"
#include "components/session_manager/core/session_manager.h"

namespace chromeos {

namespace {

// Global DemoSession instance.
DemoSession* g_demo_session = nullptr;

// Type of demo setup forced on for tests.
DemoSession::EnrollmentType g_force_enrollment_type =
    DemoSession::EnrollmentType::kNone;

// The name of the offline demo resource image loader component.
constexpr char kDemoResourcesComponentName[] = "demo-mode-resources";

// Path relative to the path at which offline demo resources are loaded that
// contains image with demo Android apps.
constexpr base::FilePath::CharType kDemoAppsPath[] =
    FILE_PATH_LITERAL("android_demo_apps.squash");

constexpr base::FilePath::CharType kExternalExtensionsPrefsPath[] =
    FILE_PATH_LITERAL("demo_extensions.json");

// Path relative to the path at which offline demo resources are loaded that
// contains the highlights app.
constexpr char kHighlightsAppPath[] = "chrome_apps/highlights/dist";

// The name of the subdirectory that contains the resources for the default
// highlights app, used when a board-specific version is not available.
constexpr char kDefaultHighlightsAppResourcesPath[] = "default";

bool IsDemoModeOfflineEnrolled() {
  DCHECK(DemoSession::IsDeviceInDemoMode());
  return DemoSession::GetEnrollmentType() ==
         DemoSession::EnrollmentType::kOffline;
}

}  // namespace

// static
base::FilePath DemoSession::GetPreInstalledDemoResourcesPath() {
  base::FilePath preinstalled_components_root;
  base::PathService::Get(DIR_PREINSTALLED_COMPONENTS,
                         &preinstalled_components_root);
  return preinstalled_components_root.AppendASCII("cros-components")
      .AppendASCII(kDemoResourcesComponentName);
}

// static
bool DemoSession::IsDeviceInDemoMode() {
  const EnrollmentType enrollment_type = GetEnrollmentType();
  return enrollment_type != EnrollmentType::kNone &&
         enrollment_type != EnrollmentType::kUnenrolled;
}

// static
DemoSession::EnrollmentType DemoSession::GetEnrollmentType() {
  if (g_force_enrollment_type != EnrollmentType::kNone)
    return g_force_enrollment_type;

  const policy::BrowserPolicyConnectorChromeOS* const connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  bool enrolled = connector->GetInstallAttributes()->GetDomain() ==
                  DemoSetupController::kDemoModeDomain;
  return enrolled ? EnrollmentType::kOnline : EnrollmentType::kUnenrolled;
}

// static
void DemoSession::SetDemoModeEnrollmentTypeForTesting(
    EnrollmentType enrollment_type) {
  g_force_enrollment_type = enrollment_type;
}

// static
void DemoSession::PreloadOfflineResourcesIfInDemoMode() {
  if (!IsDeviceInDemoMode())
    return;

  if (!g_demo_session)
    g_demo_session = new DemoSession();
  g_demo_session->EnsureOfflineResourcesLoaded(base::OnceClosure());
}

// static
DemoSession* DemoSession::StartIfInDemoMode() {
  if (!IsDeviceInDemoMode())
    return nullptr;

  if (g_demo_session && g_demo_session->started())
    return g_demo_session;

  if (!g_demo_session)
    g_demo_session = new DemoSession();

  g_demo_session->started_ = true;
  g_demo_session->EnsureOfflineResourcesLoaded(base::OnceClosure());
  return g_demo_session;
}

// static
void DemoSession::ShutDownIfInitialized() {
  if (!g_demo_session)
    return;

  DemoSession* demo_session = g_demo_session;
  g_demo_session = nullptr;
  delete demo_session;
}

// static
DemoSession* DemoSession::Get() {
  return g_demo_session;
}

void DemoSession::EnsureOfflineResourcesLoaded(
    base::OnceClosure load_callback) {
  if (offline_resources_loaded_) {
    if (load_callback)
      std::move(load_callback).Run();
    return;
  }

  if (load_callback)
    offline_resources_load_callbacks_.emplace_back(std::move(load_callback));

  if (offline_resources_load_requested_)
    return;
  offline_resources_load_requested_ = true;

  component_updater::CrOSComponentManager* cros_component_manager =
      g_browser_process->platform_part()->cros_component_manager();
  if (cros_component_manager) {
    g_browser_process->platform_part()->cros_component_manager()->Load(
        kDemoResourcesComponentName,
        component_updater::CrOSComponentManager::MountPolicy::kMount,
        component_updater::CrOSComponentManager::UpdatePolicy::kSkip,
        base::BindOnce(&DemoSession::InstalledComponentLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    // Cros component manager may be unset in tests - if that is the case,
    // report component install failure, so DemoSession attempts loading the
    // component directly from the pre-installed component path.
    InstalledComponentLoaded(
        component_updater::CrOSComponentManager::Error::INSTALL_FAILURE,
        base::FilePath());
  }
}

void DemoSession::InstalledComponentLoaded(
    component_updater::CrOSComponentManager::Error error,
    const base::FilePath& path) {
  if (error == component_updater::CrOSComponentManager::Error::NONE) {
    OnOfflineResourcesLoaded(base::make_optional(path));
    return;
  }

  chromeos::DBusThreadManager::Get()
      ->GetImageLoaderClient()
      ->LoadComponentAtPath(
          kDemoResourcesComponentName, GetPreInstalledDemoResourcesPath(),
          base::BindOnce(&DemoSession::OnOfflineResourcesLoaded,
                         weak_ptr_factory_.GetWeakPtr()));
}

base::FilePath DemoSession::GetDemoAppsPath() const {
  if (offline_resources_path_.empty())
    return base::FilePath();
  return offline_resources_path_.Append(kDemoAppsPath);
}

base::FilePath DemoSession::GetExternalExtensionsPrefsPath() const {
  if (offline_resources_path_.empty())
    return base::FilePath();
  return offline_resources_path_.Append(kExternalExtensionsPrefsPath);
}

base::FilePath DemoSession::GetOfflineResourceAbsolutePath(
    const base::FilePath& relative_path) const {
  if (offline_resources_path_.empty())
    return base::FilePath();
  if (relative_path.ReferencesParent())
    return base::FilePath();
  return offline_resources_path_.Append(relative_path);
}

DemoSession::DemoSession()
    : offline_enrolled_(IsDemoModeOfflineEnrolled()),
      session_manager_observer_(this),
      weak_ptr_factory_(this) {
  session_manager_observer_.Add(session_manager::SessionManager::Get());
  OnSessionStateChanged();
}

DemoSession::~DemoSession() = default;

void DemoSession::OnOfflineResourcesLoaded(
    base::Optional<base::FilePath> mounted_path) {
  offline_resources_loaded_ = true;

  if (mounted_path.has_value())
    offline_resources_path_ = mounted_path.value();

  std::list<base::OnceClosure> load_callbacks;
  load_callbacks.swap(offline_resources_load_callbacks_);
  for (auto& callback : load_callbacks)
    std::move(callback).Run();
}

void DemoSession::LoadAndLaunchHighlightsApp() {
  DCHECK(offline_resources_loaded_);
  if (offline_resources_path_.empty()) {
    LOG(ERROR) << "Offline resources not loaded - no highlights app available.";
    return;
  }
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  DCHECK(profile);
  const std::vector<std::string> board =
      base::SplitString(base::SysInfo::GetLsbReleaseBoard(), "-",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  const std::string board_name = board[0];
  base::FilePath resources_path =
      offline_resources_path_.Append(kHighlightsAppPath)
          .Append(kDefaultHighlightsAppResourcesPath);
  base::FilePath board_specific_resources_path =
      offline_resources_path_.Append(kHighlightsAppPath).Append(board_name);
  if (base::PathExists(board_specific_resources_path))
    resources_path = board_specific_resources_path;

  if (!apps::AppLoadService::Get(profile)->LoadAndLaunch(
          resources_path, base::CommandLine(base::CommandLine::NO_PROGRAM),
          base::FilePath() /* cur_dir */)) {
    LOG(WARNING) << "Failed to launch highlights app!";
  }
}

void DemoSession::OnSessionStateChanged() {
  if (session_manager::SessionManager::Get()->session_state() !=
          session_manager::SessionState::ACTIVE ||
      !IsDeviceInDemoMode()) {
    return;
  }
  EnsureOfflineResourcesLoaded(
      base::BindOnce(&DemoSession::LoadAndLaunchHighlightsApp,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace chromeos
