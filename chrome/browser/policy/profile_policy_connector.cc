// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "chrome/browser/policy/cloud_policy_subsystem.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/user_policy_identity_strategy.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/common/pref_names.h"

namespace {

const FilePath::CharType kPolicyDir[] = FILE_PATH_LITERAL("Device Management");
const FilePath::CharType kTokenCacheFile[] = FILE_PATH_LITERAL("Token");
const FilePath::CharType kPolicyCacheFile[] = FILE_PATH_LITERAL("Policy");

}  // namespace

namespace policy {

ProfilePolicyConnector::ProfilePolicyConnector(Profile* profile)
    : profile_(profile) {
  // TODO(mnissler): We access the file system here. The cloud policy context
  // below needs to do so anyway, since it needs to read the policy cache from
  // disk. If this proves to be a problem, we need to do this initialization
  // asynchronously on the file thread and put in synchronization that allows us
  // to wait for the cache to be read during the browser startup code paths.
  // Another option would be to provide a generic IO-safe initializer called
  // from the PrefService that we could hook up with through the policy
  // provider.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDeviceManagementUrl)) {
    FilePath policy_cache_dir(profile_->GetPath());
    policy_cache_dir = policy_cache_dir.Append(kPolicyDir);
    if (!file_util::CreateDirectory(policy_cache_dir)) {
      LOG(WARNING) << "Failed to create policy state dir "
                   << policy_cache_dir.value()
                   << ", skipping cloud policy initialization.";
      return;
    }

    identity_strategy_.reset(new UserPolicyIdentityStrategy(
        profile_,
        policy_cache_dir.Append(kTokenCacheFile)));
    cloud_policy_subsystem_.reset(new CloudPolicySubsystem(
        policy_cache_dir.Append(kPolicyCacheFile),
        identity_strategy_.get()));
  }
}

ProfilePolicyConnector::~ProfilePolicyConnector() {
  cloud_policy_subsystem_.reset();
  identity_strategy_.reset();
}

void ProfilePolicyConnector::Initialize() {
  // TODO(jkummerow, mnissler): Move this out of the browser startup path.
  if (cloud_policy_subsystem_.get()) {
    cloud_policy_subsystem_->Initialize(profile_->GetPrefs(),
                                        prefs::kPolicyUserPolicyRefreshRate,
                                        profile_->GetRequestContext());
  }
}

void ProfilePolicyConnector::Shutdown() {
  if (cloud_policy_subsystem_.get())
    cloud_policy_subsystem_->Shutdown();
}

ConfigurationPolicyProvider*
    ProfilePolicyConnector::GetManagedCloudProvider() {
  if (cloud_policy_subsystem_.get())
    return cloud_policy_subsystem_->GetManagedPolicyProvider();

  return NULL;
}

ConfigurationPolicyProvider*
    ProfilePolicyConnector::GetRecommendedCloudProvider() {
  if (cloud_policy_subsystem_.get())
    return cloud_policy_subsystem_->GetRecommendedPolicyProvider();

  return NULL;
}

// static
void ProfilePolicyConnector::RegisterPrefs(PrefService* user_prefs) {
  user_prefs->RegisterIntegerPref(prefs::kPolicyUserPolicyRefreshRate,
                                  kDefaultPolicyRefreshRateInMilliseconds);
}

}  // namespace policy
