// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_checker.h"

#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/blacklist_check.h"
#include "chrome/browser/extensions/chrome_requirements_checker.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/policy_check.h"

namespace extensions {

ExtensionInstallChecker::ExtensionInstallChecker(
    Profile* profile,
    scoped_refptr<const Extension> extension,
    int enabled_checks,
    bool fail_fast)
    : profile_(profile),
      extension_(extension),
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
  // In tests, this check may already be stubbed.
  if (!policy_check_)
    policy_check_ = base::MakeUnique<PolicyCheck>(profile_, extension_.get());
  policy_check_->Start(
      base::BindOnce(&ExtensionInstallChecker::OnManagementPolicyCheckDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ExtensionInstallChecker::OnManagementPolicyCheckDone(
    PreloadCheck::Errors errors) {
  if (!errors.empty()) {
    DCHECK_EQ(1u, errors.count(PreloadCheck::DISALLOWED_BY_POLICY));
    policy_error_ = base::UTF16ToUTF8(policy_check_->GetErrorMessage());
  }

  running_checks_ &= ~CHECK_MANAGEMENT_POLICY;
  MaybeInvokeCallback();
}

void ExtensionInstallChecker::CheckRequirements() {
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
  // In tests, this check may already be stubbed.
  if (!blacklist_check_) {
    blacklist_check_ = base::MakeUnique<BlacklistCheck>(
        Blacklist::Get(profile_), extension_.get());
  }
  blacklist_check_->Start(
      base::BindOnce(&ExtensionInstallChecker::OnBlacklistStateCheckDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ExtensionInstallChecker::OnBlacklistStateCheckDone(
    PreloadCheck::Errors errors) {
  DCHECK(is_running());

  if (errors.empty()) {
    blacklist_error_ = PreloadCheck::NONE;
  } else {
    DCHECK_EQ(1u, errors.size());
    blacklist_error_ = *errors.begin();
  }

  running_checks_ &= ~CHECK_BLACKLIST;
  MaybeInvokeCallback();
}

void ExtensionInstallChecker::MaybeInvokeCallback() {
  if (callback_.is_null())
    return;

  // Set bits for failed checks.
  int failed_mask = 0;
  if (blacklist_error_ == PreloadCheck::BLACKLISTED_ID)
    failed_mask |= CHECK_BLACKLIST;
  if (!requirement_errors_.empty())
    failed_mask |= CHECK_REQUIREMENTS;
  if (!policy_error_.empty())
    failed_mask |= CHECK_MANAGEMENT_POLICY;

  // Invoke callback if all checks are complete or there was at least one
  // failure and |fail_fast_| is true.
  if (!is_running() || (failed_mask && fail_fast_)) {
    running_checks_ = 0;

    // If we are failing fast, discard any pending results.
    blacklist_check_.reset();
    policy_check_.reset();
    requirements_checker_.reset();
    weak_ptr_factory_.InvalidateWeakPtrs();
    base::ResetAndReturn(&callback_).Run(failed_mask);
  }
}

}  // namespace extensions
