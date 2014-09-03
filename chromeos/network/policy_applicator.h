// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_POLICY_APPLICATOR_H_
#define CHROMEOS_NETWORK_POLICY_APPLICATOR_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromeos/network/network_profile.h"

namespace chromeos {

// This class compares (entry point is Run()) |modified_policies| with the
// existing entries in the provided Shill profile |profile|. It fetches all
// entries in parallel (GetProfilePropertiesCallback), compares each entry with
// the current policies (GetEntryCallback) and adds all missing policies
// (~PolicyApplicator).
class PolicyApplicator : public base::RefCounted<PolicyApplicator> {
 public:
  class ConfigurationHandler {
    public:
     virtual ~ConfigurationHandler() {}
     // Write the new configuration with the properties |shill_properties| to
     // Shill. This configuration comes from a policy. Any conflicting or
     // existing configuration for the same network will have been removed
     // before.
     virtual void CreateConfigurationFromPolicy(
         const base::DictionaryValue& shill_properties) = 0;

     virtual void UpdateExistingConfigurationWithPropertiesFromPolicy(
         const base::DictionaryValue& existing_properties,
         const base::DictionaryValue& new_properties) = 0;

     // Called after all policies were applied. At this point, the list of
     // networks should be updated.
     virtual void OnPoliciesApplied() = 0;

    private:
     DISALLOW_ASSIGN(ConfigurationHandler);
  };

  typedef std::map<std::string, const base::DictionaryValue*> GuidToPolicyMap;

  // |modified_policies| must not be NULL and will be empty afterwards.
  PolicyApplicator(base::WeakPtr<ConfigurationHandler> handler,
                   const NetworkProfile& profile,
                   const GuidToPolicyMap& all_policies,
                   const base::DictionaryValue& global_network_config,
                   std::set<std::string>* modified_policies);

  void Run();

 private:
  friend class base::RefCounted<PolicyApplicator>;

  // Called with the properties of the profile |profile_|. Requests the
  // properties of each entry, which are processed by GetEntryCallback.
  void GetProfilePropertiesCallback(
      const base::DictionaryValue& profile_properties);

  // Called with the properties of the profile entry |entry|. Checks whether the
  // entry was previously managed, whether a current policy applies and then
  // either updates, deletes or not touches the entry.
  void GetEntryCallback(const std::string& entry,
                        const base::DictionaryValue& entry_properties);

  // Sends Shill the command to delete profile entry |entry| from |profile_|.
  void DeleteEntry(const std::string& entry);

  // Sends the Shill configuration |shill_dictionary| to Shill. If |write_later|
  // is true, the configuration is queued for sending until ~PolicyApplicator.
  void WriteNewShillConfiguration(const base::DictionaryValue& shill_dictionary,
                                  const base::DictionaryValue& policy,
                                  bool write_later);

  // Called once all Profile entries are processed. Calls
  // ApplyRemainingPolicies.
  virtual ~PolicyApplicator();

  // Creates new entries for all remaining policies, i.e. for which no matching
  // Profile entry was found.
  void ApplyRemainingPolicies();

  std::set<std::string> remaining_policies_;
  base::WeakPtr<ConfigurationHandler> handler_;
  NetworkProfile profile_;
  GuidToPolicyMap all_policies_;
  base::DictionaryValue global_network_config_;
  ScopedVector<base::DictionaryValue> new_shill_configurations_;

  DISALLOW_COPY_AND_ASSIGN(PolicyApplicator);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_POLICY_APPLICATOR_H_
