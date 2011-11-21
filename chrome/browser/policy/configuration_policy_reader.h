// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_READER_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_READER_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/policy_status_info.h"
#include "policy/configuration_policy_type.h"

namespace policy {

class ConfigurationPolicyStatusKeeper;

// This class reads policy information from a ConfigurationPolicyProvider in
// order to determine the status of a policy (this includes its value and
// whether it could be enforced on the client or not), as required by the
// about:policy UI.
class ConfigurationPolicyReader : public ConfigurationPolicyProvider::Observer {
 public:

  // Observer class for the ConfigurationPolicyReader. Observer objects are
  // notified when policy values have changed.
  class Observer {
   public:
    // Called on an observer when the policy values have changed.
    virtual void OnPolicyValuesChanged() = 0;

   protected:
    virtual ~Observer() {}
  };

  ConfigurationPolicyReader(ConfigurationPolicyProvider* provider,
                            PolicyStatusInfo::PolicyLevel policy_level);
  virtual ~ConfigurationPolicyReader();

  // ConfigurationPolicyProvider::Observer methods:
  virtual void OnUpdatePolicy(ConfigurationPolicyProvider* provider) OVERRIDE;
  virtual void OnProviderGoingAway(
      ConfigurationPolicyProvider* provider) OVERRIDE;

  // Methods to handle Observers. |observer| must be non-NULL.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Creates a ConfigurationPolicyReader that reads managed platform policy.
  static ConfigurationPolicyReader* CreateManagedPlatformPolicyReader();

  // Creates a ConfigurationPolicyReader that reads managed cloud policy.
  static ConfigurationPolicyReader* CreateManagedCloudPolicyReader();

  // Creates a ConfigurationPolicyReader that reads recommended platform policy.
  static ConfigurationPolicyReader* CreateRecommendedPlatformPolicyReader();

  // Creates a ConfigurationPolicyReader that reads recommended cloud policy.
  static ConfigurationPolicyReader* CreateRecommendedCloudPolicyReader();

  // Returns a pointer to a DictionaryValue object containing policy status
  // information for the UI. Ownership of the return value is acquired by the
  // caller. Returns NULL if the reader is not aware of the given policy.
  virtual DictionaryValue*
      GetPolicyStatus(ConfigurationPolicyType policy) const;

 private:
  friend class MockConfigurationPolicyReader;

  // Only used in tests.
  ConfigurationPolicyReader();

  // Updates the policy information held in this reader. This is called when
  // the ConfigurationPolicyProvider is updated.
  void Refresh();

  // The policy provider from which policy settings are read.
  ConfigurationPolicyProvider* provider_;

  // Whether this ConfigurationPolicyReader contains managed policies.
  PolicyStatusInfo::PolicyLevel policy_level_;

  // Current policy status.
  scoped_ptr<ConfigurationPolicyStatusKeeper> policy_keeper_;

  // The list of observers for this ConfigurationPolicyReader.
  ObserverList<Observer, true> observers_;

  ConfigurationPolicyObserverRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyReader);
};

// This class combines the policy information from different
// ConfigurationPolicyReaders into a single list of policy information that the
// about:policy UI can display.
class PolicyStatus {
 public:
  typedef ConfigurationPolicyReader::Observer Observer;
  PolicyStatus(ConfigurationPolicyReader* managed_platform,
               ConfigurationPolicyReader* managed_cloud,
               ConfigurationPolicyReader* recommended_platform,
               ConfigurationPolicyReader* recommended_cloud);
  ~PolicyStatus();

  // Adds an observer to each one of the ConfigurationPolicyReaders.
  void AddObserver(Observer* observer) const;

  // Removes an observer from each one of the ConfigurationPolicyReaders.
  void RemoveObserver(Observer* observer) const;

  // Returns a ListValue pointer containing the status information of all
  // policies supported by the client. |any_policies_set| is set to true if
  // there are policies in the list that were sent by a provider, otherwise
  // it is set to false. This is for the about:policy UI to display.
  ListValue* GetPolicyStatusList(bool* any_policies_set) const;

 private:
  // Add the policy information for |policy| to the ListValue pointed to be
  // |list| as it is returned by the different ConfigurationPolicyReader
  // objects. Returns true if a policy was added and false otherwise.
  bool AddPolicyFromReaders(ConfigurationPolicyType policy,
                            ListValue* list) const;

  scoped_ptr<ConfigurationPolicyReader> managed_platform_;
  scoped_ptr<ConfigurationPolicyReader> managed_cloud_;
  scoped_ptr<ConfigurationPolicyReader> recommended_platform_;
  scoped_ptr<ConfigurationPolicyReader> recommended_cloud_;

  DISALLOW_COPY_AND_ASSIGN(PolicyStatus);
};

} // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_READER_H_
