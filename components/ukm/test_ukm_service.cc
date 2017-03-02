// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/test_ukm_service.h"

namespace ukm {

UkmServiceTestingHarness::UkmServiceTestingHarness() {
  UkmService::RegisterPrefs(test_prefs_.registry());
  test_prefs_.ClearPref(prefs::kUkmClientId);
  test_prefs_.ClearPref(prefs::kUkmSessionId);
  test_prefs_.ClearPref(prefs::kUkmPersistedLogs);

  test_ukm_service_ = base::MakeUnique<TestUkmService>(&test_prefs_);
}

UkmServiceTestingHarness::~UkmServiceTestingHarness() = default;

TestUkmService::TestUkmService(PrefService* prefs_service)
    : UkmService(prefs_service, &test_metrics_service_client_) {
  EnableRecording();
  DisableReporting();
}

TestUkmService::~TestUkmService() = default;

const UkmSource* TestUkmService::GetSource(size_t source_num) const {
  return sources_for_testing()[source_num].get();
}

const UkmEntry* TestUkmService::GetEntry(size_t entry_num) const {
  return entries_for_testing()[entry_num].get();
}

}  // namespace ukm
