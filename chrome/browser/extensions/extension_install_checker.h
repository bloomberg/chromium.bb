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
#include "extensions/browser/preload_check.h"
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

  // Returns the blacklist error of the extension. Note that there is only an
  // error if the BlacklistState is BLACKLISTED_MALWARE or BLACKLISTED_UNKNOWN.
  PreloadCheck::Error blacklist_error() const { return blacklist_error_; }

  // Returns whether management policy permits installation of the extension.
  const std::string& policy_error() const { return policy_error_; }

  void SetBlacklistCheckForTesting(std::unique_ptr<PreloadCheck> policy_check) {
    blacklist_check_ = std::move(policy_check);
  }
  void SetPolicyCheckForTesting(std::unique_ptr<PreloadCheck> policy_check) {
    policy_check_ = std::move(policy_check);
  }

 protected:
  virtual void CheckManagementPolicy();
  void OnManagementPolicyCheckDone(PreloadCheck::Errors errors);

  virtual void CheckRequirements();
  void OnRequirementsCheckDone(const std::vector<std::string>& errors);

  virtual void CheckBlacklistState();
  void OnBlacklistStateCheckDone(PreloadCheck::Errors errors);

 private:
  void MaybeInvokeCallback();

  // The Profile where the extension is being installed in.
  Profile* profile_;

  // The extension to run checks for.
  scoped_refptr<const Extension> extension_;

  // Checks requirements specified in the manifest.
  std::unique_ptr<RequirementsChecker> requirements_checker_;
  std::vector<std::string> requirement_errors_;

  // Checks if the extension is blacklisted.
  std::unique_ptr<PreloadCheck> blacklist_check_;
  PreloadCheck::Error blacklist_error_ = PreloadCheck::NONE;

  // Checks whether management policies allow the extension to be installed.
  std::unique_ptr<PreloadCheck> policy_check_;
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
