// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestions_status_service.h"

#include <string>

#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/variations/variations_associated_data.h"

namespace ntp_snippets {

namespace {

const char kFetchingRequiresSignin[] = "fetching_requires_signin";
const char kFetchingRequiresSigninEnabled[] = "true";
const char kFetchingRequiresSigninDisabled[] = "false";

}  // namespace

RemoteSuggestionsStatusService::RemoteSuggestionsStatusService(
    SigninManagerBase* signin_manager,
    PrefService* pref_service)
    : status_(RemoteSuggestionsStatus::EXPLICITLY_DISABLED),
      require_signin_(false),
      signin_manager_(signin_manager),
      pref_service_(pref_service) {
  std::string param_value_str = variations::GetVariationParamValueByFeature(
      kArticleSuggestionsFeature, kFetchingRequiresSignin);
  if (param_value_str == kFetchingRequiresSigninEnabled) {
    require_signin_ = true;
  } else if (!param_value_str.empty() &&
             param_value_str != kFetchingRequiresSigninDisabled) {
    DLOG(WARNING) << "Unknow value for the variations parameter "
                  << kFetchingRequiresSignin << ": " << param_value_str;
  }
}

RemoteSuggestionsStatusService::~RemoteSuggestionsStatusService() = default;

// static
void RemoteSuggestionsStatusService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kEnableSnippets, true);
}

void RemoteSuggestionsStatusService::Init(
    const StatusChangeCallback& callback) {
  DCHECK(status_change_callback_.is_null());

  status_change_callback_ = callback;

  // Notify about the current state before registering the observer, to make
  // sure we don't get a double notification due to an undefined start state.
  RemoteSuggestionsStatus old_status = status_;
  status_ = GetStatusFromDeps();
  status_change_callback_.Run(old_status, status_);

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kEnableSnippets,
      base::Bind(&RemoteSuggestionsStatusService::OnSnippetsEnabledChanged,
                 base::Unretained(this)));
}

void RemoteSuggestionsStatusService::OnSnippetsEnabledChanged() {
  OnStateChanged(GetStatusFromDeps());
}

void RemoteSuggestionsStatusService::OnStateChanged(
    RemoteSuggestionsStatus new_status) {
  if (new_status == status_) {
    return;
  }

  status_change_callback_.Run(status_, new_status);
  status_ = new_status;
}

bool RemoteSuggestionsStatusService::IsSignedIn() const {
  // TODO(dgn): remove the SigninManager dependency. It should be possible to
  // replace it by passing the new state via OnSignInStateChanged().
  return signin_manager_ && signin_manager_->IsAuthenticated();
}

void RemoteSuggestionsStatusService::OnSignInStateChanged() {
  OnStateChanged(GetStatusFromDeps());
}

RemoteSuggestionsStatus RemoteSuggestionsStatusService::GetStatusFromDeps()
    const {
  if (!pref_service_->GetBoolean(prefs::kEnableSnippets)) {
    DVLOG(1) << "[GetStatusFromDeps] Disabled via pref";
    return RemoteSuggestionsStatus::EXPLICITLY_DISABLED;
  }

  if (require_signin_ && !IsSignedIn()) {
    DVLOG(1) << "[GetStatusFromDeps] Signed out and disabled due to this.";
    return RemoteSuggestionsStatus::SIGNED_OUT_AND_DISABLED;
  }

  DVLOG(1) << "[GetStatusFromDeps] Enabled, signed "
           << (IsSignedIn() ? "in" : "out");
  return IsSignedIn() ? RemoteSuggestionsStatus::ENABLED_AND_SIGNED_IN
                      : RemoteSuggestionsStatus::ENABLED_AND_SIGNED_OUT;
}

}  // namespace ntp_snippets
