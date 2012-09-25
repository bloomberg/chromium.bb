// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/resource_request_allowed_notifier_test_util.h"

TestRequestAllowedNotifier::TestRequestAllowedNotifier() :
#if defined(OS_CHROMEOS)
      needs_eula_acceptance_(true),
#endif
      override_requests_allowed_(false),
      requests_allowed_(true) {
}

TestRequestAllowedNotifier::~TestRequestAllowedNotifier() {
}

#if defined(OS_CHROMEOS)
void TestRequestAllowedNotifier::SetNeedsEulaAcceptance(bool needs_acceptance) {
  needs_eula_acceptance_ = needs_acceptance;
}
#endif

void TestRequestAllowedNotifier::SetRequestsAllowedOverride(bool allowed) {
  override_requests_allowed_ = true;
  requests_allowed_ = allowed;
}

void TestRequestAllowedNotifier::NotifyObserver() {
  // Force the allowed state to true. This forces MaybeNotifyObserver to always
  // notify observers, as MaybeNotifyObserver checks ResourceRequestsAllowed.
  override_requests_allowed_ = true;
  requests_allowed_ = true;
  MaybeNotifyObserver();
}

bool TestRequestAllowedNotifier::ResourceRequestsAllowed() {
  // Call ResourceRequestAllowedNotifier::ResourceRequestsAllowed once to
  // simulate that the user requested permission. Only return that result if
  // the override flag was set.
  bool requests_allowed =
      ResourceRequestAllowedNotifier::ResourceRequestsAllowed();
  if (override_requests_allowed_)
    return requests_allowed_;
  return requests_allowed;
}

#if defined(OS_CHROMEOS)
bool TestRequestAllowedNotifier::NeedsEulaAcceptance() {
  return needs_eula_acceptance_;
}
#endif
