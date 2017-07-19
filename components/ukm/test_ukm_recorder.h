// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_TEST_UKM_RECORDER_H_
#define COMPONENTS_UKM_TEST_UKM_RECORDER_H_

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/ukm/ukm_recorder_impl.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/interfaces/ukm_interface.mojom.h"

namespace ukm {

// Wraps an UkmRecorder with additional accessors used for testing.
class TestUkmRecorder : public UkmRecorderImpl {
 public:
  TestUkmRecorder();
  ~TestUkmRecorder() override;

  size_t sources_count() const { return sources().size(); }
  const std::map<ukm::SourceId, std::unique_ptr<UkmSource>>& GetSources()
      const {
    return sources();
  }
  const UkmSource* GetSourceForUrl(const char* url) const;
  std::vector<const ukm::UkmSource*> GetSourcesForUrl(const char* url) const;
  const UkmSource* GetSourceForSourceId(ukm::SourceId source_id) const;

  size_t entries_count() const { return entries().size(); }

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

  // Returns all collected metrics for the given |source|, |event_name| and
  // |metric_name|. The order of values is not specified.
  std::vector<int64_t> GetMetrics(const UkmSource& source,
                                  const char* event_name,
                                  const char* metric_name) const;

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

  // Deprecated.
  const mojom::UkmEntry* GetEntry(size_t entry_num) const;
  // Deprecated.
  const mojom::UkmEntry* GetEntryForEntryName(const char* entry_name) const;

  // Deprecated.
  static const mojom::UkmMetric* FindMetric(const mojom::UkmEntry* entry,
                                            const char* metric_name);

 private:
  ukm::mojom::UkmEntryPtr GetMergedEntryForSourceID(
      ukm::SourceId source_id,
      const char* event_name) const;

  std::vector<const ukm::mojom::UkmEntry*> GetEntriesForSourceID(
      ukm::SourceId source_id,
      const char* event_name) const;

  DISALLOW_COPY_AND_ASSIGN(TestUkmRecorder);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_TEST_UKM_RECORDER_H_
