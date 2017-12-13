// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_TEST_UKM_RECORDER_H_
#define COMPONENTS_UKM_TEST_UKM_RECORDER_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/ukm/ukm_recorder_impl.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/interfaces/ukm_interface.mojom.h"
#include "url/gurl.h"

namespace ukm {

// Wraps an UkmRecorder with additional accessors used for testing.
class TestUkmRecorder : public UkmRecorderImpl {
 public:
  TestUkmRecorder();
  ~TestUkmRecorder() override;

  bool ShouldRestrictToWhitelistedSourceIds() const override;

  size_t sources_count() const { return sources().size(); }

  size_t entries_count() const { return entries().size(); }

  using UkmRecorderImpl::UpdateSourceURL;

  // Get all recorded UkmSource data.
  const std::map<ukm::SourceId, std::unique_ptr<UkmSource>>& GetSources()
      const {
    return sources();
  }

  // Get UkmSource data for a single SourceId.
  const UkmSource* GetSourceForSourceId(ukm::SourceId source_id) const;

  // Get all of the entries recorded for entry name.
  std::vector<const mojom::UkmEntry*> GetEntriesByName(
      base::StringPiece entry_name) const;

  // Get the data for all entries with given entry name, merged to one entry
  // for each source id. Intended for singular="true" metrics.
  std::map<ukm::SourceId, mojom::UkmEntryPtr> GetMergedEntriesByName(
      base::StringPiece entry_name) const;

  // Check if an entry is associated with a url.
  void ExpectEntrySourceHasUrl(const mojom::UkmEntry* entry,
                               const GURL& url) const;

  // Expect the value of a metric from an entry.
  static void ExpectEntryMetric(const mojom::UkmEntry* entry,
                                base::StringPiece metric_name,
                                int64_t expected_value);

  // Check if an entry contains a specific metric.
  static bool EntryHasMetric(const mojom::UkmEntry* entry,
                             base::StringPiece metric_name);

  // Expect the value of a metric from an entry.
  static const int64_t* GetEntryMetric(const mojom::UkmEntry* entry,
                                       base::StringPiece metric_name);

  // All of the methods below are deprecated.
  // Use GetEntriesByName based testing instead.
  // TODO(crbug/761524): Migrate tests and remove these.

  // Get all SourceIds with any data associated with them.
  std::set<ukm::SourceId> GetSourceIds() const;

  // Returns the UKM source for the given URL. If there are multiple sources for
  // the given URL, this returns the first source that is created. If there is
  // no source for the given URL, this returns nullptr.
  const UkmSource* GetSourceForUrl(const char* url) const;
  const UkmSource* GetSourceForUrl(const GURL& url) const {
    return GetSourceForUrl(url.spec().c_str());
  }
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

  // Returns all recorded values of a metric for the given |source|,
  // |event_name| and |metric_name|. The order of values is not specified.
  std::vector<int64_t> GetMetricValues(ukm::SourceId source_id,
                                       const char* event_name,
                                       const char* metric_name) const;

  // Deprecated.
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
                                            base::StringPiece metric_name);

 private:
  ukm::mojom::UkmEntryPtr GetMergedEntryForSourceID(
      ukm::SourceId source_id,
      const char* event_name) const;

  std::vector<const ukm::mojom::UkmEntry*> GetEntriesForSourceID(
      ukm::SourceId source_id,
      const char* event_name) const;

  DISALLOW_COPY_AND_ASSIGN(TestUkmRecorder);
};

// Similar to a TestUkmRecorder, but also sets itself as the global UkmRecorder
// on construction, and unsets itself on destruction.
class TestAutoSetUkmRecorder : public TestUkmRecorder {
 public:
  TestAutoSetUkmRecorder();
  ~TestAutoSetUkmRecorder() override;

 private:
  base::WeakPtrFactory<TestAutoSetUkmRecorder> self_ptr_factory_;
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_TEST_UKM_RECORDER_H_
