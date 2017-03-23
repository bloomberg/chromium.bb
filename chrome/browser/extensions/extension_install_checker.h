// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_CHECKER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_CHECKER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "extensions/browser/blacklist_state.h"
#include "extensions/common/extension.h"

class Profile;

namespace extensions {

class RequirementsChecker;

// Performs common checks for validating whether an extension may be installed.
// This class should be Start()-ed at most once.
class ExtensionInstallChecker {
 public:
  // Called when checks are complete. The returned value is a bitmask of
  // failed checks.
  typedef base::Callback<void(int)> Callback;

  enum CheckType {
    // Check the blacklist state of the extension.
    CHECK_BLACKLIST = 1 << 0,
    // Check whether the extension has requirement errors.
    CHECK_REQUIREMENTS = 1 << 1,
    // Check whether the extension can be installed and loaded, according to
    // management policies.
    CHECK_MANAGEMENT_POLICY = 1 << 2,
    // Perform all checks.
    CHECK_ALL = (1 << 3) - 1
  };

  // |enabled_checks| is a bitmask of CheckTypes to run.
  // If |fail_fast| is true, the callback to Start() will be invoked once any
  // check fails. Otherwise it will be invoked when all checks have completed.
  ExtensionInstallChecker(Profile* profile,
                          scoped_refptr<const Extension> extension,
                          int enabled_checks,
                          bool fail_fast);
  virtual ~ExtensionInstallChecker();

  // Starts the set of checks. |callback| will only be called once.
  // This function must be called on the UI thread. The callback also occurs on
  // the UI thread. Checks may run asynchronously in parallel.
  // This function should be invoked at most once.
  void Start(const Callback& callback);

  // Returns true if any checks are currently running.
  bool is_running() const { return running_checks_ != 0; }

  // Returns the requirement violations. A non-empty list is considered to be
  // a check failure.
  const std::vector<std::string>& requirement_errors() const {
    return requirement_errors_;
  }

  // Returns the blacklist state of the extension. A blacklist state of
  // BLACKLISTED_MALWARE is considered to be a check failure.
  BlacklistState blacklist_state() const { return blacklist_state_; }

  // Returns whether management policy permits installation of the extension.
  bool policy_allows_load() const { return policy_allows_load_; }
  const std::string& policy_error() const { return policy_error_; }

 protected:
  virtual void CheckManagementPolicy();
  void OnManagementPolicyCheckDone(bool allows_load, const std::string& error);

  virtual void CheckRequirements();
  void OnRequirementsCheckDone(const std::vector<std::string>& errors);

  virtual void CheckBlacklistState();
  void OnBlacklistStateCheckDone(BlacklistState state);

 private:
  void MaybeInvokeCallback();

  std::unique_ptr<RequirementsChecker> requirements_checker_;

  // The Profile where the extension is being installed in.
  Profile* profile_;

  // The extension to run checks for.
  scoped_refptr<const Extension> extension_;

  // Requirement violations.
  std::vector<std::string> requirement_errors_;

  // Result of the blacklist state check.
  BlacklistState blacklist_state_;

  // Whether the extension can be installed, according to management policies.
  bool policy_allows_load_;
  std::string policy_error_;

  // Bitmask of enabled checks.
  int enabled_checks_;

  // Bitmask of currently running checks.
  // TODO(michaelpg): Consolidate this with enabled_checks_.
  int running_checks_;

  // If true, the callback is invoked when the first check fails.
  bool fail_fast_;

  // The callback to invoke when checks are complete.
  Callback callback_;

  base::WeakPtrFactory<ExtensionInstallChecker> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallChecker);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_CHECKER_H_
