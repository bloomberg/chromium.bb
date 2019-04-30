// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STARTUP_DATA_H_
#define CHROME_BROWSER_STARTUP_DATA_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace policy {
class ConfigurationPolicyProvider;
class ProfilePolicyConnector;
class SchemaRegistryService;
}  // namespace policy

namespace sync_preferences {
class PrefServiceSyncable;
}

class PrefService;
class ProfileKey;
class ChromeFeatureListCreator;

// The StartupData owns any pre-created objects in //chrome before the full
// browser starts, including the ChromeFeatureListCreator and the Profile's
// PrefService. See doc:
// https://docs.google.com/document/d/1ybmGWRWXu0aYNxA99IcHFesDAslIaO1KFP6eGdHTJaE/edit#heading=h.7bk05syrcom
class StartupData {
 public:
  StartupData();
  ~StartupData();

#if defined(OS_ANDROID)
  // Initializes all necessary parameters to create the Profile's PrefService.
  void CreateProfilePrefService();

  // Returns whether a PrefService has been created.
  bool HasBuiltProfilePrefService();

  ProfileKey* GetProfileKey();

  // Passes ownership of the |key_| to the caller.
  std::unique_ptr<ProfileKey> TakeProfileKey();

  // Passes ownership of the |schema_registry_service_| to the caller.
  std::unique_ptr<policy::SchemaRegistryService> TakeSchemaRegistryService();

  // Passes ownership of the |configuration_policy_provider_| to the caller.
  std::unique_ptr<policy::ConfigurationPolicyProvider>
  TakeConfigurationPolicyProvider();

  // Passes ownership of the |profile_policy_connector_| to the caller.
  std::unique_ptr<policy::ProfilePolicyConnector> TakeProfilePolicyConnector();

  // Passes ownership of the |pref_registry_| to the caller.
  scoped_refptr<user_prefs::PrefRegistrySyncable> TakePrefRegistrySyncable();

  // Passes ownership of the |prefs_| to the caller.
  std::unique_ptr<sync_preferences::PrefServiceSyncable>
  TakeProfilePrefService();
#endif

  ChromeFeatureListCreator* chrome_feature_list_creator() {
    return chrome_feature_list_creator_.get();
  }

 private:
#if defined(OS_ANDROID)
  void PreProfilePrefServiceInit();
  void CreateProfilePrefServiceInternal();

  std::unique_ptr<ProfileKey> key_;

  std::unique_ptr<policy::SchemaRegistryService> schema_registry_service_;
  std::unique_ptr<policy::ConfigurationPolicyProvider>
      configuration_policy_provider_;
  std::unique_ptr<policy::ProfilePolicyConnector> profile_policy_connector_;

  scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry_;

  std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs_;
#endif

  std::unique_ptr<ChromeFeatureListCreator> chrome_feature_list_creator_;

  DISALLOW_COPY_AND_ASSIGN(StartupData);
};

#endif  // CHROME_BROWSER_STARTUP_DATA_H_
