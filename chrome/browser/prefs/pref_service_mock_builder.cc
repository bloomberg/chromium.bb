// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_service_mock_builder.h"

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "base/prefs/default_pref_store.h"
#include "base/prefs/json_pref_store.h"
#include "base/prefs/testing_pref_store.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/browser/prefs/pref_notifier_impl.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

PrefServiceMockBuilder::PrefServiceMockBuilder() {
  ResetTestingState();
}

PrefServiceMockBuilder::~PrefServiceMockBuilder() {}

#if defined(ENABLE_CONFIGURATION_POLICY)
PrefServiceMockBuilder& PrefServiceMockBuilder::WithManagedPolicies(
    policy::PolicyService* service) {
  WithManagedPrefs(new policy::ConfigurationPolicyPrefStore(
      service, policy::POLICY_LEVEL_MANDATORY));
  return *this;
}

PrefServiceMockBuilder& PrefServiceMockBuilder::WithRecommendedPolicies(
    policy::PolicyService* service) {
  WithRecommendedPrefs(new policy::ConfigurationPolicyPrefStore(
      service, policy::POLICY_LEVEL_RECOMMENDED));
  return *this;
}
#endif

PrefServiceMockBuilder&
PrefServiceMockBuilder::WithCommandLine(CommandLine* command_line) {
  WithCommandLinePrefs(new CommandLinePrefStore(command_line));
  return *this;
}

PrefService* PrefServiceMockBuilder::Create() {
  PrefService* pref_service = PrefServiceBuilder::Create();
  ResetTestingState();
  return pref_service;
}

void PrefServiceMockBuilder::ResetTestingState() {
  user_prefs_ = new TestingPrefStore;
}
