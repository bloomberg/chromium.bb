// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_notifier.h"

namespace policy {

void PolicyNotifier::AddObserver(CloudPolicySubsystem::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void PolicyNotifier::RemoveObserver(CloudPolicySubsystem::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

PolicyNotifier::PolicyNotifier()
    : state_(CloudPolicySubsystem::UNENROLLED),
      error_details_(CloudPolicySubsystem::NO_DETAILS) {
  for (int i = 0; i < NUM_SOURCES; ++i) {
    component_states_[i] = CloudPolicySubsystem::UNENROLLED;
    component_error_details_[i] = CloudPolicySubsystem::NO_DETAILS;
  }
}

PolicyNotifier::~PolicyNotifier() {
}

void PolicyNotifier::Inform(PolicySubsystemState state,
                            ErrorDetails error_details,
                            StatusSource source) {
  component_states_[source] = state;
  component_error_details_[source] = error_details;
  RecomputeState();
}

void PolicyNotifier::RecomputeState() {
  // Define shortcuts.
  PolicySubsystemState* s = component_states_;
  ErrorDetails* e = component_error_details_;

  // Compute overall state. General idea: If any component knows we're
  // unmanaged, set that as global state. Otherwise, ask components in the
  // order they normally do work in. If anyone reports 'SUCCESS' or 'UNENROLLED'
  // (which can also be read as 'undefined/unknown', ask the next component.
  if (s[TOKEN_FETCHER] == CloudPolicySubsystem::UNMANAGED ||
      s[POLICY_CONTROLLER] == CloudPolicySubsystem::UNMANAGED ||
      s[POLICY_CACHE] == CloudPolicySubsystem::UNMANAGED) {
    state_ = CloudPolicySubsystem::UNMANAGED;
    error_details_ = CloudPolicySubsystem::NO_DETAILS;
  } else if (s[TOKEN_FETCHER] == CloudPolicySubsystem::NETWORK_ERROR) {
    state_ = s[TOKEN_FETCHER];
    error_details_ = e[TOKEN_FETCHER];
  } else if (s[TOKEN_FETCHER] ==  CloudPolicySubsystem::BAD_GAIA_TOKEN) {
    state_ = s[TOKEN_FETCHER];
    error_details_ = e[TOKEN_FETCHER];
  } else if (s[POLICY_CONTROLLER] == CloudPolicySubsystem::NETWORK_ERROR) {
    state_ = s[POLICY_CONTROLLER];
    error_details_ = e[POLICY_CONTROLLER];
  } else if (s[TOKEN_FETCHER] == CloudPolicySubsystem::SUCCESS &&
             s[POLICY_CONTROLLER] != CloudPolicySubsystem::SUCCESS) {
    // We need to be able to differentiate between token fetch success or
    // policy fetch success.
    state_ = CloudPolicySubsystem::TOKEN_FETCHED;
    error_details_ = CloudPolicySubsystem::NO_DETAILS;
  } else {
    state_ = s[POLICY_CACHE];
    error_details_ = e[POLICY_CACHE];
  }

  FOR_EACH_OBSERVER(CloudPolicySubsystem::Observer, observer_list_,
                    OnPolicyStateChanged(state_, error_details_));
}

}  // namespace policy
