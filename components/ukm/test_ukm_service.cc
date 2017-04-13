// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/test_ukm_service.h"

#include "base/logging.h"
#include "base/metrics/metrics_hashes.h"
#include "components/ukm/ukm_entry.h"
#include "components/ukm/ukm_source.h"

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

const std::map<int32_t, std::unique_ptr<UkmSource>>&
TestUkmService::GetSources() const {
  return sources_for_testing();
}

const UkmSource* TestUkmService::GetSourceForUrl(const char* url) const {
  const UkmSource* source = nullptr;
  for (const auto& kv : sources_for_testing()) {
    if (kv.second->url() == url) {
      DCHECK_EQ(nullptr, source);
      source = kv.second.get();
    }
  }
  return source;
}

const UkmSource* TestUkmService::GetSourceForSourceId(int32_t source_id) const {
  const UkmSource* source = nullptr;
  for (const auto& kv : sources_for_testing()) {
    if (kv.second->id() == source_id) {
      DCHECK_EQ(nullptr, source);
      source = kv.second.get();
    }
  }
  return source;
}

const UkmEntry* TestUkmService::GetEntry(size_t entry_num) const {
  return entries_for_testing()[entry_num].get();
}

const UkmEntry* TestUkmService::GetEntryForEntryName(
    const char* entry_name) const {
  for (const auto& it : entries_for_testing()) {
    if (it->event_hash() == base::HashMetricName(entry_name))
      return it.get();
  }
  return nullptr;
}

}  // namespace ukm
