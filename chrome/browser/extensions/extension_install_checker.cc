// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_checker.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/requirements_checker.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"

namespace extensions {

ExtensionInstallChecker::ExtensionInstallChecker(Profile* profile)
    : profile_(profile),
      blacklist_state_(NOT_BLACKLISTED),
      policy_allows_load_(true),
      current_sequence_number_(0),
      running_checks_(0),
      fail_fast_(false),
      weak_ptr_factory_(this) {
}

ExtensionInstallChecker::~ExtensionInstallChecker() {
}

void ExtensionInstallChecker::Start(int enabled_checks,
                                    bool fail_fast,
                                    const Callback& callback) {
  // Profile is null in tests.
  if (profile_) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!extension_.get()) {
      NOTREACHED();
      return;
    }
  }

  if (is_running() || !enabled_checks || callback.is_null()) {
    NOTREACHED();
    return;
  }

  running_checks_ = enabled_checks;
  fail_fast_ = fail_fast;
  callback_ = callback;
  ResetResults();

  // Execute the management policy check first as it is synchronous.
  if (enabled_checks & CHECK_MANAGEMENT_POLICY) {
    CheckManagementPolicy();
    if (!is_running())
      return;
  }

  if (enabled_checks & CHECK_REQUIREMENTS) {
    CheckRequirements();
    if (!is_running())
      return;
  }

  if (enabled_checks & CHECK_BLACKLIST)
    CheckBlacklistState();
}

void ExtensionInstallChecker::CheckManagementPolicy() {
  DCHECK(extension_.get());

  base::string16 error;
  bool allow = ExtensionSystem::Get(profile_)->management_policy()->UserMayLoad(
      extension_.get(), &error);
  OnManagementPolicyCheckDone(allow, base::UTF16ToUTF8(error));
}

void ExtensionInstallChecker::OnManagementPolicyCheckDone(
    bool allows_load,
    const std::string& error) {
  policy_allows_load_ = allows_load;
  policy_error_ = error;
  DCHECK(policy_allows_load_ || !policy_error_.empty());

  running_checks_ &= ~CHECK_MANAGEMENT_POLICY;
  MaybeInvokeCallback();
}

void ExtensionInstallChecker::CheckRequirements() {
  DCHECK(extension_.get());

  if (!requirements_checker_.get())
    requirements_checker_.reset(new RequirementsChecker());
  requirements_checker_->Check(
      extension_,
      base::Bind(&ExtensionInstallChecker::OnRequirementsCheckDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 current_sequence_number_));
}

void ExtensionInstallChecker::OnRequirementsCheckDone(
    int sequence_number,
    std::vector<std::string> errors) {
  // Some pending results may arrive after fail fast.
  if (sequence_number != current_sequence_number_)
    return;

  requirement_errors_ = errors;

  running_checks_ &= ~CHECK_REQUIREMENTS;
  MaybeInvokeCallback();
}

void ExtensionInstallChecker::CheckBlacklistState() {
  DCHECK(extension_.get());

  extensions::Blacklist* blacklist =
      ExtensionSystem::Get(profile_)->blacklist();
  blacklist->IsBlacklisted(
      extension_->id(),
      base::Bind(&ExtensionInstallChecker::OnBlacklistStateCheckDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 current_sequence_number_));
}

void ExtensionInstallChecker::OnBlacklistStateCheckDone(int sequence_number,
                                                        BlacklistState state) {
  // Some pending results may arrive after fail fast.
  if (sequence_number != current_sequence_number_)
    return;

  blacklist_state_ = state;

  running_checks_ &= ~CHECK_BLACKLIST;
  MaybeInvokeCallback();
}

void ExtensionInstallChecker::ResetResults() {
  requirement_errors_.clear();
  blacklist_state_ = NOT_BLACKLISTED;
  policy_allows_load_ = true;
  policy_error_.clear();
}

void ExtensionInstallChecker::MaybeInvokeCallback() {
  if (callback_.is_null())
    return;

  // Set bits for failed checks.
  int failed_mask = 0;
  if (blacklist_state_ == BLACKLISTED_MALWARE)
    failed_mask |= CHECK_BLACKLIST;
  if (!requirement_errors_.empty())
    failed_mask |= CHECK_REQUIREMENTS;
  if (!policy_allows_load_)
    failed_mask |= CHECK_MANAGEMENT_POLICY;

  // Invoke callback if all checks are complete or there was at least one
  // failure and |fail_fast_| is true.
  if (!is_running() || (failed_mask && fail_fast_)) {
    // If we are failing fast, discard any pending results.
    weak_ptr_factory_.InvalidateWeakPtrs();
    running_checks_ = 0;
    ++current_sequence_number_;

    Callback callback_copy = callback_;
    callback_.Reset();

    // This instance may be owned by the callback recipient and deleted here,
    // so reset |callback_| first and invoke a copy of the callback.
    callback_copy.Run(failed_mask);
  }
}

}  // namespace extensions
