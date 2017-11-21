// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_util.h"

#include <string>

#include "ash/public/cpp/app_types.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/user_manager/user_manager.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

namespace arc {

namespace {

// This is for finch. See also crbug.com/633704 for details.
// TODO(hidehiko): More comments of the intention how this works, when
// we unify the commandline flags.
const base::Feature kEnableArcFeature{"EnableARC",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Possible values for --arc-availability flag.
constexpr char kAvailabilityNone[] = "none";
constexpr char kAvailabilityInstalled[] = "installed";
constexpr char kAvailabilityOfficiallySupported[] = "officially-supported";
constexpr char kAlwaysStart[] = "always-start";
constexpr char kAlwaysStartWithNoPlayStore[] =
    "always-start-with-no-play-store";
constexpr char kOnlyStartAfterLogin[] = "only-start-after-login";

void SetArcCpuRestrictionCallback(
    login_manager::ContainerCpuRestrictionState state,
    bool success) {
  if (success)
    return;
  const char* message =
      (state == login_manager::CONTAINER_CPU_RESTRICTION_BACKGROUND)
          ? "unprioritize"
          : "prioritize";
  LOG(ERROR) << "Failed to " << message << " ARC";
}

}  // namespace

bool IsArcAvailable() {
  const auto* command_line = base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(chromeos::switches::kArcAvailability)) {
    const std::string value =
        command_line->GetSwitchValueASCII(chromeos::switches::kArcAvailability);
    DCHECK(value == kAvailabilityNone || value == kAvailabilityInstalled ||
           value == kAvailabilityOfficiallySupported)
        << "Unknown flag value: " << value;
    return value == kAvailabilityOfficiallySupported ||
           (value == kAvailabilityInstalled &&
            base::FeatureList::IsEnabled(kEnableArcFeature));
  }

  // For transition, fallback to old flags.
  // TODO(hidehiko): Remove this and clean up whole this function, when
  // session_manager supports a new flag.
  return command_line->HasSwitch(chromeos::switches::kEnableArc) ||
         (command_line->HasSwitch(chromeos::switches::kArcAvailable) &&
          base::FeatureList::IsEnabled(kEnableArcFeature));
}

bool IsWebstoreSearchEnabled() {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(chromeos::switches::kArcAvailability)) {
    const std::string value =
        command_line->GetSwitchValueASCII(chromeos::switches::kArcAvailability);

    return value == kAvailabilityNone;
  }
  return true;
}

bool IsPlayStoreAvailable() {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(chromeos::switches::kArcStartMode))
    return true;

  const std::string value =
      command_line->GetSwitchValueASCII(chromeos::switches::kArcStartMode);
  return value != kAlwaysStartWithNoPlayStore;
}

bool ShouldArcAlwaysStart() {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(chromeos::switches::kArcStartMode))
    return false;
  const std::string value =
      command_line->GetSwitchValueASCII(chromeos::switches::kArcStartMode);
  return value == kAlwaysStartWithNoPlayStore || value == kAlwaysStart;
}

void SetArcAlwaysStartForTesting(bool play_store_available) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kArcStartMode,
      play_store_available ? kAlwaysStart : kAlwaysStartWithNoPlayStore);
}

bool ShouldArcOnlyStartAfterLogin() {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(chromeos::switches::kArcStartMode))
    return false;
  const std::string value =
      command_line->GetSwitchValueASCII(chromeos::switches::kArcStartMode);
  return value == kOnlyStartAfterLogin;
}

bool IsArcKioskAvailable() {
  const auto* command_line = base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(chromeos::switches::kArcAvailability)) {
    std::string value =
        command_line->GetSwitchValueASCII(chromeos::switches::kArcAvailability);
    if (value == kAvailabilityInstalled)
      return true;
    return IsArcAvailable();
  }

  // TODO(hidehiko): Remove this when session_manager supports the new flag.
  if (command_line->HasSwitch(chromeos::switches::kArcAvailable))
    return true;

  // If not special kiosk device case, use general ARC check.
  return IsArcAvailable();
}

void SetArcAvailableCommandLineForTesting(base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(chromeos::switches::kArcAvailability,
                                  kAvailabilityOfficiallySupported);
}

bool IsArcKioskMode() {
  return user_manager::UserManager::IsInitialized() &&
         user_manager::UserManager::Get()->IsLoggedInAsArcKioskApp();
}

bool IsArcAllowedForUser(const user_manager::User* user) {
  if (!user) {
    VLOG(1) << "No ARC for nullptr user.";
    return false;
  }

  // ARC is only supported for the following cases:
  // - Users have Gaia accounts;
  // - Active directory users;
  // - ARC kiosk session;
  //   USER_TYPE_ARC_KIOSK_APP check is compatible with IsArcKioskMode()
  //   above because ARC kiosk user is always the primary/active user of a
  //   user session.
  if (!user->HasGaiaAccount() && !user->IsActiveDirectoryUser() &&
      user->GetType() != user_manager::USER_TYPE_ARC_KIOSK_APP) {
    VLOG(1) << "Users without GAIA or AD accounts, or not ARC kiosk apps are "
               "not supported in ARC.";
    return false;
  }

  // Do not allow for ephemeral data user. cf) b/26402681
  if (user_manager::UserManager::Get()->IsUserCryptohomeDataEphemeral(
          user->GetAccountId())) {
    VLOG(1) << "Users with ephemeral data are not supported in ARC.";
    return false;
  }

  if (user->GetType() == user_manager::USER_TYPE_CHILD) {
    VLOG(1) << "ARC usage by Child users is prohibited";
    return false;
  }

  return true;
}

bool IsArcOptInVerificationDisabled() {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(
      chromeos::switches::kDisableArcOptInVerification);
}

bool IsArcAppWindow(aura::Window* window) {
  if (!window)
    return false;
  return window->GetProperty(aura::client::kAppType) ==
         static_cast<int>(ash::AppType::ARC_APP);
}

void SetArcCpuRestriction(bool do_restrict) {
  chromeos::SessionManagerClient* session_manager_client =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  if (!session_manager_client) {
    LOG(WARNING) << "SessionManagerClient is not available";
    return;
  }
  const login_manager::ContainerCpuRestrictionState state =
      do_restrict ? login_manager::CONTAINER_CPU_RESTRICTION_BACKGROUND
                  : login_manager::CONTAINER_CPU_RESTRICTION_FOREGROUND;
  session_manager_client->SetArcCpuRestriction(
      state, base::Bind(SetArcCpuRestrictionCallback, state));
}

}  // namespace arc
