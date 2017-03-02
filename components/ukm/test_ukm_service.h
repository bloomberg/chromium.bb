// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_TEST_UKM_SERVICE_H_
#define COMPONENTS_UKM_TEST_UKM_SERVICE_H_

#include <stddef.h>
#include <memory>

#include "components/metrics/test_metrics_service_client.h"
#include "components/prefs/testing_pref_service.h"
#include "components/ukm/ukm_pref_names.h"
#include "components/ukm/ukm_service.h"

namespace ukm {

// Wraps an UkmService with additional accessors used for testing.
class TestUkmService : public UkmService {
 public:
  explicit TestUkmService(PrefService* pref_service);
  ~TestUkmService() override;

  size_t sources_count() const { return sources_for_testing().size(); }
  const UkmSource* GetSource(size_t source_num) const;

  size_t entries_count() const { return entries_for_testing().size(); }
  const UkmEntry* GetEntry(size_t entry_num) const;

 private:
  metrics::TestMetricsServiceClient test_metrics_service_client_;

  DISALLOW_COPY_AND_ASSIGN(TestUkmService);
};

// Convenience harness used for testing; creates a TestUkmService and
// supplies it with a prefs service with a longer lifespan.
class UkmServiceTestingHarness {
 public:
  UkmServiceTestingHarness();
  virtual ~UkmServiceTestingHarness();

  TestUkmService* test_ukm_service() { return test_ukm_service_.get(); }

 private:
  TestingPrefServiceSimple test_prefs_;
  std::unique_ptr<TestUkmService> test_ukm_service_;

  DISALLOW_COPY_AND_ASSIGN(UkmServiceTestingHarness);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_TEST_UKM_SERVICE_H_
