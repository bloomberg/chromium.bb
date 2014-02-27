// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_app_launcher.h"

#include "base/command_line.h"
#include "chrome/browser/chromeos/app_mode/app_session_lifetime.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/browser/extension_system.h"
#include "grit/browser_resources.h"

namespace chromeos {

const char DemoAppLauncher::kDemoUserName[] = "demouser@demo.app.local";

DemoAppLauncher::DemoAppLauncher() : profile_(NULL) {}

DemoAppLauncher::~DemoAppLauncher() {}

void DemoAppLauncher::StartDemoAppLaunch() {
  DVLOG(1) << "Launching demo app...";
  // user_id = DemoAppUserId, force_emphemeral = true, delegate = this.
  kiosk_profile_loader_.reset(
      new KioskProfileLoader(kDemoUserName, true, this));
  kiosk_profile_loader_->Start();
}

// static
bool DemoAppLauncher::IsDemoAppSession(const std::string& user_id) {
  return user_id == kDemoUserName ? true : false;
}

void DemoAppLauncher::OnProfileLoaded(Profile* profile) {
  DVLOG(1) << "Profile loaded... Starting demo app launch.";
  profile_ = profile;

  kiosk_profile_loader_.reset();

  // Load our demo app, then launch it.
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  std::string extension_id = extension_service->component_loader()->Add(
      IDR_DEMO_APP_MANIFEST,
      base::FilePath("/usr/share/chromeos-assets/demo_app"));

  const extensions::Extension* extension =
      extension_service->GetExtensionById(extension_id, true);

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kForceAppMode);
  command_line->AppendSwitchASCII(switches::kAppId, extension_id);

  OpenApplication(AppLaunchParams(
      profile_, extension, extensions::LAUNCH_CONTAINER_WINDOW, NEW_WINDOW));
  InitAppSession(profile_, extension_id);

  UserManager::Get()->SessionStarted();

  LoginDisplayHostImpl::default_host()->Finalize();
}

void DemoAppLauncher::OnProfileLoadFailed(KioskAppLaunchError::Error error) {
  LOG(ERROR) << "Loading the Kiosk Profile failed: " <<
      KioskAppLaunchError::GetErrorMessage(error);
}

}  // namespace chromeos
