// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_metrics.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"

namespace {

ProfileMetrics::ProfileType GetProfileType(
    FilePath& profile_path) {
  ProfileMetrics::ProfileType metric = ProfileMetrics::SECONDARY;
  ProfileManager* manager = g_browser_process->profile_manager();
  FilePath user_data_dir;
  // In unittests, we do not always have a profile_manager so check.
  if (manager) {
    user_data_dir = manager->user_data_dir();
  }
  if (profile_path == user_data_dir.AppendASCII(chrome::kInitialProfile)) {
    metric = ProfileMetrics::ORIGINAL;
  }
  return metric;
}

}  // namespace

void ProfileMetrics::LogProfileAvatarSelection(size_t icon_index) {
  DCHECK(icon_index < NUM_PROFILE_AVATAR_METRICS);
  ProfileAvatar icon_name;
  switch (icon_index) {
    case 0:
      icon_name = AVATAR_GENERIC;
    case 1:
      icon_name = AVATAR_GENERIC_AQUA;
    case 2:
      icon_name = AVATAR_GENERIC_BLUE;
    case 3:
      icon_name = AVATAR_GENERIC_GREEN;
    case 4:
      icon_name = AVATAR_GENERIC_ORANGE;
    case 5:
      icon_name = AVATAR_GENERIC_PURPLE;
    case 6:
      icon_name = AVATAR_GENERIC_RED;
    case 7:
      icon_name = AVATAR_GENERIC_YELLOW;
    case 8:
      icon_name = AVATAR_SECRET_AGENT;
    case 9:
      icon_name = AVATAR_SUPERHERO;
    case 10:
      icon_name = AVATAR_VOLLEYBALL;
    case 11:
      icon_name = AVATAR_BUSINESSMAN;
    case 12:
      icon_name = AVATAR_NINJA;
    case 13:
      icon_name = AVATAR_ALIEN;
    case 14:
      icon_name = AVATAR_AWESOME;
    case 15:
      icon_name = AVATAR_FLOWER;
    case 16:
      icon_name = AVATAR_PIZZA;
    case 17:
      icon_name = AVATAR_SOCCER;
    case 18:
      icon_name = AVATAR_BURGER;
    case 19:
      icon_name = AVATAR_CAT;
    case 20:
      icon_name = AVATAR_CUPCAKE;
    case 21:
      icon_name = AVATAR_DOG;
    case 22:
      icon_name = AVATAR_HORSE;
    case 23:
      icon_name = AVATAR_MARGARITA;
    case 24:
      icon_name = AVATAR_NOTE;
    case 25:
      icon_name = AVATAR_SUN_CLOUD;
    default:  // We should never actually get here, but just in case
      icon_name = AVATAR_UNKNOWN;
  }
  UMA_HISTOGRAM_ENUMERATION("Profile.Avatar", icon_name,
                            NUM_PROFILE_AVATAR_METRICS);
}

void ProfileMetrics::LogProfileOpenMethod(ProfileOpen metric) {
  DCHECK(metric < NUM_PROFILE_OPEN_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Profile.Opening", metric,
                            NUM_PROFILE_OPEN_METRICS);
}

void ProfileMetrics::LogProfileSyncInfo(ProfileSync metric) {
  DCHECK(metric < NUM_PROFILE_SYNC_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Profile.Sync", metric,
                            NUM_PROFILE_SYNC_METRICS);
}

void ProfileMetrics::LogProfileUpdate(FilePath& profile_path) {
  UMA_HISTOGRAM_ENUMERATION("Profile.Update",
                            GetProfileType(profile_path),
                            NUM_PROFILE_TYPE_METRICS);
}

void ProfileMetrics::LogProfileSyncSignIn(FilePath& profile_path) {
  ProfileSync metric = SYNC_SIGN_IN_ORIGINAL;
  if (GetProfileType(profile_path) == SECONDARY) {
    metric = SYNC_SIGN_IN_SECONDARY;
  }
  UMA_HISTOGRAM_ENUMERATION("Profile.Sync", SYNC_SIGN_IN,
                            NUM_PROFILE_SYNC_METRICS);
  UMA_HISTOGRAM_ENUMERATION("Profile.Sync", metric,
                            NUM_PROFILE_SYNC_METRICS);
}
