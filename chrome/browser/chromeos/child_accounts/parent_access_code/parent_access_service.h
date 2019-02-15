// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_PARENT_ACCESS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_PARENT_ACCESS_SERVICE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/chromeos/child_accounts/parent_access_code/config_source.h"
#include "components/account_id/account_id.h"

namespace base {
class Clock;
}  // namespace base

namespace chromeos {
namespace parent_access {

class Authenticator;

// Parent access code validation service.
class ParentAccessService : public ConfigSource::Observer {
 public:
  // Delegate that gets notified about attempts to validate parent access code.
  class Delegate {
   public:
    Delegate();
    virtual ~Delegate();

    // Called when access code validation was performed. |result| is true if
    // validation finished with a success and false otherwise.
    virtual void OnAccessCodeValidation(bool result) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  explicit ParentAccessService(std::unique_ptr<ConfigSource> config_source);
  ~ParentAccessService() override;

  void SetDelegate(Delegate* delegate);

  // ConfigSource::Observer:
  void OnConfigChanged(const ConfigSource::ConfigSet& configs) override;

  // Allows to override the time for testing purposes.
  void SetClockForTesting(base::Clock* clock);

 private:
  friend class ParentAccessServiceTest;

  void CreateValidators(const ConfigSource::ConfigSet& configs);

  // Delegate that will be notified about results of access code validation.
  Delegate* delegate_ = nullptr;

  // Provides configurations to be used for validation of access codes.
  std::unique_ptr<ConfigSource> config_source_;

  // Authenticators used for validation of parent access code.
  // There is one validator per active parent access code configuration.
  // Validators should be used in the order they are stored in
  // |access_code_validators_|. Validation is considered successful if any of
  // validators returns success.
  std::vector<std::unique_ptr<Authenticator>> access_code_validators_;

  // Points to the base::DefaultClock by default.
  const base::Clock* clock_;

  DISALLOW_COPY_AND_ASSIGN(ParentAccessService);
};

}  // namespace parent_access
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_PARENT_ACCESS_SERVICE_H_
