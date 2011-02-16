// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/policy/configuration_policy_store_interface.h"
#include "policy/configuration_policy_type.h"

namespace policy {

// A mostly-abstract super class for platform-specific policy providers.
// Platform-specific policy providers (Windows Group Policy, gconf,
// etc.) should implement a subclass of this class.
class ConfigurationPolicyProvider {
 public:
  typedef std::map<ConfigurationPolicyType, Value*> PolicyMapType;

  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnUpdatePolicy() = 0;
    virtual void OnProviderGoingAway() = 0;
  };

  // Used for static arrays of policy values that is used to initialize an
  // instance of the ConfigurationPolicyProvider.
  struct PolicyDefinitionList {
    struct Entry {
      ConfigurationPolicyType policy_type;
      Value::ValueType value_type;
      const char* name;
    };

    const Entry* begin;
    const Entry* end;
  };

  explicit ConfigurationPolicyProvider(const PolicyDefinitionList* policy_list);

  virtual ~ConfigurationPolicyProvider();

  // Must be implemented by provider subclasses to specify the provider-specific
  // policy decisions. The preference service invokes this |Provide| method when
  // it needs a policy provider to specify its policy choices. In |Provide|, the
  // |ConfigurationPolicyProvider| must make calls to the |Apply| method of
  // |store| to apply specific policies. Returns true if the policy could be
  // provided, otherwise false.
  virtual bool Provide(ConfigurationPolicyStoreInterface* store) = 0;

  // Check whether this provider has completed initialization. This is used to
  // detect whether initialization is done in case providers implementations
  // need to do asynchronous operations for initialization.
  virtual bool IsInitializationComplete() const;

 protected:
  // Decodes the value tree and writes the configuration to the given |store|.
  void ApplyPolicyValueTree(const DictionaryValue* policies,
                             ConfigurationPolicyStoreInterface* store);

  // Writes the configuration found in the already-decoded map |policies| to
  // the given |store|.
  void ApplyPolicyMap(const PolicyMapType* policies,
                      ConfigurationPolicyStoreInterface* store);

  const PolicyDefinitionList* policy_definition_list() const {
    return policy_definition_list_;
  }

 private:
  friend class ConfigurationPolicyObserverRegistrar;

  virtual void AddObserver(ConfigurationPolicyProvider::Observer* observer) = 0;
  virtual void RemoveObserver(
      ConfigurationPolicyProvider::Observer* observer) = 0;

  // Contains the default mapping from policy values to the actual names.
  const ConfigurationPolicyProvider::PolicyDefinitionList*
      policy_definition_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyProvider);
};

// Manages observers for a ConfigurationPolicyProvider. Is used to register
// observers, and automatically removes them upon destruction.
// Implementation detail: to avoid duplicate bookkeeping of registered
// observers, this registrar class acts as a proxy for notifications (since it
// needs to register itself anyway to get OnProviderGoingAway notifications).
class ConfigurationPolicyObserverRegistrar
    : ConfigurationPolicyProvider::Observer {
 public:
  ConfigurationPolicyObserverRegistrar();
  ~ConfigurationPolicyObserverRegistrar();
  void Init(ConfigurationPolicyProvider* provider,
            ConfigurationPolicyProvider::Observer* observer);

  // ConfigurationPolicyProvider::Observer implementation:
  virtual void OnUpdatePolicy();
  virtual void OnProviderGoingAway();

 private:
  ConfigurationPolicyProvider* provider_;
  ConfigurationPolicyProvider::Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyObserverRegistrar);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_H_
