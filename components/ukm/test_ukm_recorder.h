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

  // Get the value of a metric from an entry, null if not present.
  static const int64_t* GetEntryMetric(const mojom::UkmEntry* entry,
                                       base::StringPiece metric_name);

 private:
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
