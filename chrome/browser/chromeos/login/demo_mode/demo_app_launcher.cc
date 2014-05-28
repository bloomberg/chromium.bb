// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_app_launcher.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/app_mode/app_session_lifetime.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "grit/browser_resources.h"
#include "ui/base/window_open_disposition.h"

namespace chromeos {

const char DemoAppLauncher::kDemoUserName[] = "demouser@demo.app.local";
const char DemoAppLauncher::kDemoAppId[] = "klimoghijjogocdbaikffefjfcfheiel";
const base::FilePath::CharType kDefaultDemoAppPath[] =
    FILE_PATH_LITERAL("/usr/share/chromeos-assets/demo_app");

// static
base::FilePath* DemoAppLauncher::demo_app_path_ = NULL;

DemoAppLauncher::DemoAppLauncher() {
  if (!demo_app_path_)
    demo_app_path_ = new base::FilePath(kDefaultDemoAppPath);
}

DemoAppLauncher::~DemoAppLauncher() {
  delete demo_app_path_;
}

void DemoAppLauncher::StartDemoAppLaunch() {
  DVLOG(1) << "Launching demo app...";
  // user_id = DemoAppUserId, force_emphemeral = true, delegate = this.
  kiosk_profile_loader_.reset(
      new KioskProfileLoader(kDemoUserName, true, this));
  kiosk_profile_loader_->Start();
}

// static
bool DemoAppLauncher::IsDemoAppSession(const std::string& user_id) {
  return user_id == kDemoUserName;
}

// static
void DemoAppLauncher::SetDemoAppPathForTesting(const base::FilePath& path) {
  delete demo_app_path_;
  demo_app_path_ = new base::FilePath(path);
}

void DemoAppLauncher::OnProfileLoaded(Profile* profile) {
  DVLOG(1) << "Profile loaded... Starting demo app launch.";

  kiosk_profile_loader_.reset();

  // Load our demo app, then launch it.
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  CHECK(demo_app_path_);
  const std::string extension_id = extension_service->component_loader()->Add(
      IDR_DEMO_APP_MANIFEST,
      *demo_app_path_);

  const extensions::Extension* extension =
      extension_service->GetExtensionById(extension_id, true);
  if (!extension) {
    // We've already done too much setup at this point to just return out, it
    // is safer to just restart.
    chrome::AttemptUserExit();
    return;
  }

  // Disable network before launching the app.
  LOG(WARNING) << "Disabling network before launching demo app..";
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  handler->SetTechnologyEnabled(NetworkTypePattern::NonVirtual(),
                                false,
                                chromeos::network_handler::ErrorCallback());

  OpenApplication(AppLaunchParams(
      profile, extension, extensions::LAUNCH_CONTAINER_WINDOW, NEW_WINDOW));
  InitAppSession(profile, extension_id);

  UserManager::Get()->SessionStarted();

  LoginDisplayHostImpl::default_host()->Finalize();
}

void DemoAppLauncher::OnProfileLoadFailed(KioskAppLaunchError::Error error) {
  LOG(ERROR) << "Loading the Kiosk Profile failed: " <<
      KioskAppLaunchError::GetErrorMessage(error);
}

}  // namespace chromeos
