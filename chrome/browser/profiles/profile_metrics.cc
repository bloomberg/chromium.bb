// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_metrics.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_header_helper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"

namespace {

const int kMaximumReportedProfileCount = 5;
const int kMaximumDaysOfDisuse = 4 * 7;  // Should be integral number of weeks.

ProfileMetrics::ProfileType GetProfileType(
    const base::FilePath& profile_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  ProfileMetrics::ProfileType metric = ProfileMetrics::SECONDARY;
  ProfileManager* manager = g_browser_process->profile_manager();
  base::FilePath user_data_dir;
  // In unittests, we do not always have a profile_manager so check.
  if (manager) {
    user_data_dir = manager->user_data_dir();
  }
  if (profile_path == user_data_dir.AppendASCII(chrome::kInitialProfile)) {
    metric = ProfileMetrics::ORIGINAL;
  }
  return metric;
}

void UpdateReportedOSProfileStatistics(int active, int signedin) {
#if defined(OS_WIN)
  GoogleUpdateSettings::UpdateProfileCounts(active, signedin);
#endif
}

void LogLockedProfileInformation(ProfileManager* manager) {
  const ProfileInfoCache& info_cache = manager->GetProfileInfoCache();
  size_t number_of_profiles = info_cache.GetNumberOfProfiles();

  base::Time now = base::Time::Now();
  const int kMinutesInProfileValidDuration =
      base::TimeDelta::FromDays(28).InMinutes();
  for (size_t i = 0; i < number_of_profiles; ++i) {
    // Find when locked profiles were locked
    if (info_cache.ProfileIsSigninRequiredAtIndex(i)) {
      base::TimeDelta time_since_lock = now -
          info_cache.GetProfileActiveTimeAtIndex(i);
      // Specifying 100 buckets for the histogram to get a higher level of
      // granularity in the reported data, given the large number of possible
      // values (kMinutesInProfileValidDuration > 40,000).
      UMA_HISTOGRAM_CUSTOM_COUNTS("Profile.LockedProfilesDuration",
                                  time_since_lock.InMinutes(),
                                  1,
                                  kMinutesInProfileValidDuration,
                                  100);
    }
  }
}

bool HasProfileAtIndexBeenActiveSince(const ProfileInfoCache& info_cache,
                                      int index,
                                      const base::Time& active_limit) {
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  // TODO(mlerman): iOS and Android should set an ActiveTime in the
  // ProfileInfoCache. (see ProfileManager::OnBrowserSetLastActive)
  if (info_cache.GetProfileActiveTimeAtIndex(index) < active_limit)
    return false;
#endif
  return true;
}

}  // namespace

enum ProfileAvatar {
  AVATAR_GENERIC = 0,       // The names for avatar icons
  AVATAR_GENERIC_AQUA,
  AVATAR_GENERIC_BLUE,
  AVATAR_GENERIC_GREEN,
  AVATAR_GENERIC_ORANGE,
  AVATAR_GENERIC_PURPLE,
  AVATAR_GENERIC_RED,
  AVATAR_GENERIC_YELLOW,
  AVATAR_SECRET_AGENT,
  AVATAR_SUPERHERO,
  AVATAR_VOLLEYBALL,        // 10
  AVATAR_BUSINESSMAN,
  AVATAR_NINJA,
  AVATAR_ALIEN,
  AVATAR_AWESOME,
  AVATAR_FLOWER,
  AVATAR_PIZZA,
  AVATAR_SOCCER,
  AVATAR_BURGER,
  AVATAR_CAT,
  AVATAR_CUPCAKE,           // 20
  AVATAR_DOG,
  AVATAR_HORSE,
  AVATAR_MARGARITA,
  AVATAR_NOTE,
  AVATAR_SUN_CLOUD,
  AVATAR_PLACEHOLDER,
  AVATAR_UNKNOWN,           // 27
  AVATAR_GAIA,              // 28
  NUM_PROFILE_AVATAR_METRICS
};

bool ProfileMetrics::CountProfileInformation(ProfileManager* manager,
                                             ProfileCounts* counts) {
  const ProfileInfoCache& info_cache = manager->GetProfileInfoCache();
  size_t number_of_profiles = info_cache.GetNumberOfProfiles();
  counts->total = number_of_profiles;

  // Ignore other metrics if we have no profiles, e.g. in Chrome Frame tests.
  if (!number_of_profiles)
    return false;

  // Maximum age for "active" profile is 4 weeks.
  base::Time oldest = base::Time::Now() -
      base::TimeDelta::FromDays(kMaximumDaysOfDisuse);

  for (size_t i = 0; i < number_of_profiles; ++i) {
    if (!HasProfileAtIndexBeenActiveSince(info_cache, i, oldest)) {
      counts->unused++;
    } else {
      if (info_cache.ProfileIsSupervisedAtIndex(i))
        counts->supervised++;
      if (!info_cache.GetUserNameOfProfileAtIndex(i).empty()) {
        counts->signedin++;
        if (info_cache.IsUsingGAIAPictureOfProfileAtIndex(i))
          counts->gaia_icon++;
      }
    }
  }
  return true;
}


void ProfileMetrics::UpdateReportedProfilesStatistics(ProfileManager* manager) {
  ProfileCounts counts;
  if (CountProfileInformation(manager, &counts)) {
    int limited_total = counts.total;
    int limited_signedin = counts.signedin;
    if (limited_total > kMaximumReportedProfileCount) {
      limited_total = kMaximumReportedProfileCount + 1;
      limited_signedin =
          (int)((float)(counts.signedin * limited_total)
          / counts.total + 0.5);
    }
    UpdateReportedOSProfileStatistics(limited_total, limited_signedin);
  }
}

void ProfileMetrics::LogNumberOfProfiles(ProfileManager* manager) {
  ProfileCounts counts;
  bool success = CountProfileInformation(manager, &counts);
  UMA_HISTOGRAM_COUNTS_100("Profile.NumberOfProfiles", counts.total);

  // Ignore other metrics if we have no profiles, e.g. in Chrome Frame tests.
  if (success) {
    UMA_HISTOGRAM_COUNTS_100("Profile.NumberOfManagedProfiles",
                             counts.supervised);
    UMA_HISTOGRAM_COUNTS_100("Profile.PercentageOfManagedProfiles",
                             100 * counts.supervised / counts.total);
    UMA_HISTOGRAM_COUNTS_100("Profile.NumberOfSignedInProfiles",
                             counts.signedin);
    UMA_HISTOGRAM_COUNTS_100("Profile.NumberOfUnusedProfiles",
                             counts.unused);
    UMA_HISTOGRAM_COUNTS_100("Profile.NumberOfSignedInProfilesWithGAIAIcons",
                             counts.gaia_icon);

    LogLockedProfileInformation(manager);
    UpdateReportedOSProfileStatistics(counts.total, counts.signedin);
  }
}

void ProfileMetrics::LogProfileAddNewUser(ProfileAdd metric) {
  DCHECK(metric < NUM_PROFILE_ADD_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Profile.AddNewUser", metric,
                            NUM_PROFILE_ADD_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Profile.NetUserCount", ADD_NEW_USER,
                            NUM_PROFILE_NET_METRICS);
}

void ProfileMetrics::LogProfileAvatarSelection(size_t icon_index) {
  DCHECK(icon_index < NUM_PROFILE_AVATAR_METRICS);
  ProfileAvatar icon_name = AVATAR_UNKNOWN;
  switch (icon_index) {
    case 0:
      icon_name = AVATAR_GENERIC;
      break;
    case 1:
      icon_name = AVATAR_GENERIC_AQUA;
      break;
    case 2:
      icon_name = AVATAR_GENERIC_BLUE;
      break;
    case 3:
      icon_name = AVATAR_GENERIC_GREEN;
      break;
    case 4:
      icon_name = AVATAR_GENERIC_ORANGE;
      break;
    case 5:
      icon_name = AVATAR_GENERIC_PURPLE;
      break;
    case 6:
      icon_name = AVATAR_GENERIC_RED;
      break;
    case 7:
      icon_name = AVATAR_GENERIC_YELLOW;
      break;
    case 8:
      icon_name = AVATAR_SECRET_AGENT;
      break;
    case 9:
      icon_name = AVATAR_SUPERHERO;
      break;
    case 10:
      icon_name = AVATAR_VOLLEYBALL;
      break;
    case 11:
      icon_name = AVATAR_BUSINESSMAN;
      break;
    case 12:
      icon_name = AVATAR_NINJA;
      break;
    case 13:
      icon_name = AVATAR_ALIEN;
      break;
    case 14:
      icon_name = AVATAR_AWESOME;
      break;
    case 15:
      icon_name = AVATAR_FLOWER;
      break;
    case 16:
      icon_name = AVATAR_PIZZA;
      break;
    case 17:
      icon_name = AVATAR_SOCCER;
      break;
    case 18:
      icon_name = AVATAR_BURGER;
      break;
    case 19:
      icon_name = AVATAR_CAT;
      break;
    case 20:
      icon_name = AVATAR_CUPCAKE;
      break;
    case 21:
      icon_name = AVATAR_DOG;
      break;
    case 22:
      icon_name = AVATAR_HORSE;
      break;
    case 23:
      icon_name = AVATAR_MARGARITA;
      break;
    case 24:
      icon_name = AVATAR_NOTE;
      break;
    case 25:
      icon_name = AVATAR_SUN_CLOUD;
      break;
    case 26:
      icon_name = AVATAR_PLACEHOLDER;
      break;
    case 28:
      icon_name = AVATAR_GAIA;
      break;
    default:  // We should never actually get here.
      NOTREACHED();
      break;
  }
  UMA_HISTOGRAM_ENUMERATION("Profile.Avatar", icon_name,
                            NUM_PROFILE_AVATAR_METRICS);
}

void ProfileMetrics::LogProfileDeleteUser(ProfileNetUserCounts metric) {
  DCHECK(metric < NUM_PROFILE_NET_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Profile.NetUserCount", metric,
                            NUM_PROFILE_NET_METRICS);
}

void ProfileMetrics::LogProfileOpenMethod(ProfileOpen metric) {
  DCHECK(metric < NUM_PROFILE_OPEN_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Profile.OpenMethod", metric,
                            NUM_PROFILE_OPEN_METRICS);
}

void ProfileMetrics::LogProfileSwitchGaia(ProfileGaia metric) {
  if (metric == GAIA_OPT_IN)
    LogProfileAvatarSelection(AVATAR_GAIA);
  UMA_HISTOGRAM_ENUMERATION("Profile.SwitchGaiaPhotoSettings",
                            metric,
                            NUM_PROFILE_GAIA_METRICS);
}

void ProfileMetrics::LogProfileSwitchUser(ProfileOpen metric) {
  DCHECK(metric < NUM_PROFILE_OPEN_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Profile.OpenMethod", metric,
                            NUM_PROFILE_OPEN_METRICS);
}

void ProfileMetrics::LogProfileSyncInfo(ProfileSync metric) {
  DCHECK(metric < NUM_PROFILE_SYNC_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Profile.SyncCustomize", metric,
                            NUM_PROFILE_SYNC_METRICS);
}

void ProfileMetrics::LogProfileAuthResult(ProfileAuth metric) {
  UMA_HISTOGRAM_ENUMERATION("Profile.AuthResult", metric,
                            NUM_PROFILE_AUTH_METRICS);
}

void ProfileMetrics::LogProfileUpgradeEnrollment(
    ProfileUpgradeEnrollment metric) {
  UMA_HISTOGRAM_ENUMERATION("Profile.UpgradeEnrollment", metric,
                            NUM_PROFILE_ENROLLMENT_METRICS);
}

void ProfileMetrics::LogProfileDesktopMenu(
    ProfileDesktopMenu metric,
    signin::GAIAServiceType gaia_service) {
  // The first parameter to the histogram needs to be literal, because of the
  // optimized implementation of |UMA_HISTOGRAM_ENUMERATION|. Do not attempt
  // to refactor.
  switch (gaia_service) {
    case signin::GAIA_SERVICE_TYPE_NONE:
      UMA_HISTOGRAM_ENUMERATION("Profile.DesktopMenu.NonGAIA", metric,
                                NUM_PROFILE_DESKTOP_MENU_METRICS);
      break;
    case signin::GAIA_SERVICE_TYPE_SIGNOUT:
      UMA_HISTOGRAM_ENUMERATION("Profile.DesktopMenu.GAIASignout", metric,
                                NUM_PROFILE_DESKTOP_MENU_METRICS);
      break;
    case signin::GAIA_SERVICE_TYPE_INCOGNITO:
      UMA_HISTOGRAM_ENUMERATION("Profile.DesktopMenu.GAIAIncognito",
                                metric, NUM_PROFILE_DESKTOP_MENU_METRICS);
      break;
    case signin::GAIA_SERVICE_TYPE_ADDSESSION:
      UMA_HISTOGRAM_ENUMERATION("Profile.DesktopMenu.GAIAAddSession", metric,
                                NUM_PROFILE_DESKTOP_MENU_METRICS);
      break;
    case signin::GAIA_SERVICE_TYPE_REAUTH:
      UMA_HISTOGRAM_ENUMERATION("Profile.DesktopMenu.GAIAReAuth", metric,
                                NUM_PROFILE_DESKTOP_MENU_METRICS);
      break;
    case signin::GAIA_SERVICE_TYPE_DEFAULT:
      UMA_HISTOGRAM_ENUMERATION("Profile.DesktopMenu.GAIADefault", metric,
                                NUM_PROFILE_DESKTOP_MENU_METRICS);
      break;
  }
}

void ProfileMetrics::LogProfileDelete(bool profile_was_signed_in) {
  UMA_HISTOGRAM_BOOLEAN("Profile.Delete", profile_was_signed_in);
}

#if defined(OS_ANDROID)
void ProfileMetrics::LogProfileAndroidAccountManagementMenu(
    ProfileAndroidAccountManagementMenu metric,
    signin::GAIAServiceType gaia_service) {
  // The first parameter to the histogram needs to be literal, because of the
  // optimized implementation of |UMA_HISTOGRAM_ENUMERATION|. Do not attempt
  // to refactor.
  switch (gaia_service) {
    case signin::GAIA_SERVICE_TYPE_NONE:
      UMA_HISTOGRAM_ENUMERATION(
          "Profile.AndroidAccountManagementMenu.NonGAIA",
          metric,
          NUM_PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU_METRICS);
      break;
    case signin::GAIA_SERVICE_TYPE_SIGNOUT:
      UMA_HISTOGRAM_ENUMERATION(
          "Profile.AndroidAccountManagementMenu.GAIASignout",
          metric,
          NUM_PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU_METRICS);
      break;
    case signin::GAIA_SERVICE_TYPE_INCOGNITO:
      UMA_HISTOGRAM_ENUMERATION(
          "Profile.AndroidAccountManagementMenu.GAIASignoutIncognito",
          metric,
          NUM_PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU_METRICS);
      break;
    case signin::GAIA_SERVICE_TYPE_ADDSESSION:
      UMA_HISTOGRAM_ENUMERATION(
          "Profile.AndroidAccountManagementMenu.GAIAAddSession",
          metric,
          NUM_PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU_METRICS);
      break;
    case signin::GAIA_SERVICE_TYPE_REAUTH:
      UMA_HISTOGRAM_ENUMERATION(
          "Profile.AndroidAccountManagementMenu.GAIAReAuth",
          metric,
          NUM_PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU_METRICS);
      break;
    case signin::GAIA_SERVICE_TYPE_DEFAULT:
      UMA_HISTOGRAM_ENUMERATION(
          "Profile.AndroidAccountManagementMenu.GAIADefault",
          metric,
          NUM_PROFILE_ANDROID_ACCOUNT_MANAGEMENT_MENU_METRICS);
      break;
  }
}
#endif  // defined(OS_ANDROID)

void ProfileMetrics::LogProfileLaunch(Profile* profile) {
  base::FilePath profile_path = profile->GetPath();
  UMA_HISTOGRAM_ENUMERATION("Profile.LaunchBrowser",
                            GetProfileType(profile_path),
                            NUM_PROFILE_TYPE_METRICS);

  if (profile->IsSupervised()) {
    content::RecordAction(
        base::UserMetricsAction("ManagedMode_NewManagedUserWindow"));
  }
}

void ProfileMetrics::LogProfileSyncSignIn(const base::FilePath& profile_path) {
  UMA_HISTOGRAM_ENUMERATION("Profile.SyncSignIn",
                            GetProfileType(profile_path),
                            NUM_PROFILE_TYPE_METRICS);
}

void ProfileMetrics::LogProfileUpdate(const base::FilePath& profile_path) {
  UMA_HISTOGRAM_ENUMERATION("Profile.Update",
                            GetProfileType(profile_path),
                            NUM_PROFILE_TYPE_METRICS);
}
