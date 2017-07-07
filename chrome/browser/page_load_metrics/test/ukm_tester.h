// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_TEST_UKM_TESTER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_TEST_UKM_TESTER_H_

#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/interfaces/ukm_interface.mojom.h"

namespace ukm {
class UkmSource;
}  // namespace ukm

namespace page_load_metrics {
namespace test {

// UkmTester provides utilities for testing the logging of UKM sources, events,
// and metrics.
class UkmTester {
 public:
  explicit UkmTester(ukm::TestUkmRecorder* test_ukm_recorder);

  // Number of UKM sources recorded.
  size_t sources_count() const { return test_ukm_recorder_->sources_count(); }

  // Number of UKM entries recorded.
  size_t entries_count() const { return test_ukm_recorder_->entries_count(); }

  const ukm::UkmSource* GetSourceForUrl(const char* url) const;

  std::vector<const ukm::UkmSource*> GetSourcesForUrl(const char* url) const;

  // Returns whether the given UKM |source| has an entry with the given
  // |event_name|.
  bool HasEntry(const ukm::UkmSource& source,
                const char* event_name) const WARN_UNUSED_RESULT;

  // Returns the number of metrics recorded for the given UKM |source| and
  // |event_name|.
  int CountMetricsForEventName(const ukm::UkmSource& source,
                               const char* event_name) const;

  // Returns whether a metric with the given |metric_name| was recorded for the
  // given UKM |source| and |event_name|.
  bool HasMetric(const ukm::UkmSource& source,
                 const char* event_name,
                 const char* metric_name) const WARN_UNUSED_RESULT;

  // Returns the number of metrics recorded with the given |metric_name|, for
  // the given UKM |source| and |event_name|.
  int CountMetrics(const ukm::UkmSource& source,
                   const char* event_name,
                   const char* metric_name) const WARN_UNUSED_RESULT;

  // Expects that a single metric was recorded with the given |expected_value|,
  // for the given |source| and |event_name|. This is shorthand for calling
  // ExpectMetrics with a single value in the expected values vector.
  void ExpectMetric(const ukm::UkmSource& source,
                    const char* event_name,
                    const char* metric_name,
                    int64_t expected_value) const;

  // Expects that metrics were recorded with the given |expected_values|, for
  // the given |source| and |event_name|. The order of |expected_values| is not
  // significant.
  void ExpectMetrics(const ukm::UkmSource& source,
                     const char* event_name,
                     const char* metric_name,
                     const std::vector<int64_t>& expected_values) const;

  // UKM entries that may be recorded multiple times for a single source may
  // need to verify that an expected number of UKM entries were logged. The
  // utility methods below can be used to verify expected entries. UKM entries
  // that aren't recorded multiple times per source should prefer using
  // HasMetric/CountMetrics/ExpectMetric(s) above.

  // Returns the number of entries recorded with the given |event_name|, for the
  // given UKM |source|.
  int CountEntries(const ukm::UkmSource& source, const char* event_name) const;

  // Expects that an entry with the given |expected_metrics| was recorded, for
  // the given |source| and |event_name|. The order of |expected_metrics| is not
  // significant. |expected_metrics| contains (metric name,metric value) pairs.
  void ExpectEntry(const ukm::UkmSource& source,
                   const char* event_name,
                   const std::vector<std::pair<const char*, int64_t>>&
                       expected_metrics) const;

 private:
  ukm::mojom::UkmEntryPtr GetMergedEntryForSourceID(
      ukm::SourceId source_id,
      const char* event_name) const;

  std::vector<const ukm::mojom::UkmEntry*> GetEntriesForSourceID(
      ukm::SourceId source_id,
      const char* event_name) const;

  const ukm::TestUkmRecorder* test_ukm_recorder_;

  DISALLOW_COPY_AND_ASSIGN(UkmTester);
};

}  // namespace test
}  // namespace page_load_metrics

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_TEST_UKM_TESTER_H_
