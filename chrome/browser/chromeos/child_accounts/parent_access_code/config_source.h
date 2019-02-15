// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_CONFIG_SOURCE_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_CONFIG_SOURCE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/child_accounts/parent_access_code/authenticator.h"

namespace chromeos {
namespace parent_access {

class AccessCodeConfig;

// Base class for parent access code configuration providers.
class ConfigSource {
 public:
  // Set of parent access code configurations used for generation and validation
  // of parent access code.
  struct ConfigSet {
    ConfigSet();
    ConfigSet(ConfigSet&&);
    ConfigSet& operator=(ConfigSet&&);
    ~ConfigSet();

    // Primary config used for validation of parent access code.
    base::Optional<AccessCodeConfig> future_config;

    // Primary config used for generation of parent access code.
    // Should be used for validation only when access code cannot be validated
    // with |future_config|.
    base::Optional<AccessCodeConfig> current_config;

    // These configs should be used for validation of parent access code only
    // when it cannot be validated with |future_config| nor |current_config|.
    std::vector<AccessCodeConfig> old_configs;

   private:
    DISALLOW_COPY_AND_ASSIGN(ConfigSet);
  };

  // Observer interface for ConfigSource that gets notified when parent access
  // code configuration changed.
  class Observer : public base::CheckedObserver {
   public:
    Observer();
    ~Observer() override;

    // Called when parent access code configuration changed.
    virtual void OnConfigChanged(const ConfigSet& configs) = 0;
  };

  ConfigSource();
  virtual ~ConfigSource();

  // Returns current parent access code configurations.
  virtual ConfigSet GetConfigSet() = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  void NotifyConfigChanged(const ConfigSet& configs);

 private:
  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(ConfigSource);
};

}  // namespace parent_access
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_CONFIG_SOURCE_H_
