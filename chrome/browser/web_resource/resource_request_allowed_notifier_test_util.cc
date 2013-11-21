// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_resource/resource_request_allowed_notifier_test_util.h"

TestRequestAllowedNotifier::TestRequestAllowedNotifier()
    : override_requests_allowed_(false),
      requests_allowed_(true) {
}

TestRequestAllowedNotifier::~TestRequestAllowedNotifier() {
}

void TestRequestAllowedNotifier::InitWithEulaAcceptNotifier(
    Observer* observer,
    scoped_ptr<EulaAcceptedNotifier> eula_notifier) {
  test_eula_notifier_.swap(eula_notifier);
  Init(observer);
}

void TestRequestAllowedNotifier::SetRequestsAllowedOverride(bool allowed) {
  override_requests_allowed_ = true;
  requests_allowed_ = allowed;
}

void TestRequestAllowedNotifier::NotifyObserver() {
  // Force the allowed state and requested state to true. This forces
  // MaybeNotifyObserver to always notify observers, as MaybeNotifyObserver
  // checks ResourceRequestsAllowed and requested state.
  override_requests_allowed_ = true;
  requests_allowed_ = true;
  SetObserverRequestedForTesting(true);
  MaybeNotifyObserver();
}

ResourceRequestAllowedNotifier::State
    TestRequestAllowedNotifier::GetResourceRequestsAllowedState() {
  if (override_requests_allowed_)
    return requests_allowed_ ? ALLOWED : DISALLOWED_NETWORK_DOWN;
  return ResourceRequestAllowedNotifier::GetResourceRequestsAllowedState();
}

EulaAcceptedNotifier* TestRequestAllowedNotifier::CreateEulaNotifier() {
  return test_eula_notifier_.release();
}
