// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_checker.h"

#include "base/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/chrome_requirements_checker.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"

namespace extensions {

ExtensionInstallChecker::ExtensionInstallChecker(
    Profile* profile,
    scoped_refptr<const Extension> extension,
    int enabled_checks,
    bool fail_fast)
    : profile_(profile),
      extension_(extension),
      blacklist_state_(NOT_BLACKLISTED),
      policy_allows_load_(true),
      enabled_checks_(enabled_checks),
      running_checks_(0),
      fail_fast_(fail_fast),
      weak_ptr_factory_(this) {}

ExtensionInstallChecker::~ExtensionInstallChecker() {
}

void ExtensionInstallChecker::Start(const Callback& callback) {
  // Profile is null in tests.
  if (profile_) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // TODO(michaelpg): change NOTREACHED to [D]CHECK here and below.
    if (!extension_.get()) {
      NOTREACHED();
      return;
    }
  }

  if (is_running() || !enabled_checks_ || callback.is_null()) {
    NOTREACHED();
    return;
  }

  running_checks_ = enabled_checks_;
  callback_ = callback;

  // Execute the management policy check first as it is synchronous.
  if (enabled_checks_ & CHECK_MANAGEMENT_POLICY) {
    CheckManagementPolicy();
    if (!is_running())
      return;
  }

  if (enabled_checks_ & CHECK_REQUIREMENTS) {
    CheckRequirements();
    if (!is_running())
      return;
  }

  if (enabled_checks_ & CHECK_BLACKLIST)
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

  requirements_checker_ = base::MakeUnique<ChromeRequirementsChecker>();
  requirements_checker_->Check(
      extension_, base::Bind(&ExtensionInstallChecker::OnRequirementsCheckDone,
                             weak_ptr_factory_.GetWeakPtr()));
}

void ExtensionInstallChecker::OnRequirementsCheckDone(
    const std::vector<std::string>& errors) {
  DCHECK(is_running());

  requirement_errors_ = errors;

  running_checks_ &= ~CHECK_REQUIREMENTS;
  MaybeInvokeCallback();
}

void ExtensionInstallChecker::CheckBlacklistState() {
  DCHECK(extension_.get());

  extensions::Blacklist* blacklist = Blacklist::Get(profile_);
  blacklist->IsBlacklisted(
      extension_->id(),
      base::Bind(&ExtensionInstallChecker::OnBlacklistStateCheckDone,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ExtensionInstallChecker::OnBlacklistStateCheckDone(BlacklistState state) {
  DCHECK(is_running());

  blacklist_state_ = state;

  running_checks_ &= ~CHECK_BLACKLIST;
  MaybeInvokeCallback();
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
    base::ResetAndReturn(&callback_).Run(failed_mask);
  }
}

}  // namespace extensions
