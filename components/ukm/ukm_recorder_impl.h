// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_UKM_RECORDER_IMPL_H_
#define COMPONENTS_UKM_UKM_RECORDER_IMPL_H_

#include <map>
#include <set>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"

namespace metrics {
class UkmBrowserTest;
class UkmEGTestHelper;
}

namespace ukm {

class UkmSource;
class Report;

namespace debug {
class UkmDebugDataExtractor;
}

class UkmRecorderImpl : public UkmRecorder {
  using IsWebstoreExtensionCallback =
      base::RepeatingCallback<bool(base::StringPiece id)>;

 public:
  UkmRecorderImpl();
  ~UkmRecorderImpl() override;

  // Enables/disables recording control if data is allowed to be collected. The
  // |extensions| flag separately controls recording of chrome-extension://
  // URLs; this flag should reflect the "sync extensions" user setting.
  void EnableRecording(bool extensions);
  void DisableRecording();

  // Deletes stored recordings.
  void Purge();

  // Sets a callback for determining if an extension URL can be recorded.
  void SetIsWebstoreExtensionCallback(
      const IsWebstoreExtensionCallback& callback);

 protected:
  // Cache the list of whitelisted entries from the field trial parameter.
  void StoreWhitelistedEntries();

  // Writes recordings into a report proto, and clears recordings.
  void StoreRecordingsInReport(Report* report);

  const std::map<SourceId, std::unique_ptr<UkmSource>>& sources() const {
    return sources_;
  }

  const std::vector<mojom::UkmEntryPtr>& entries() const { return entries_; }

  // UkmRecorder:
  void UpdateSourceURL(SourceId source_id, const GURL& url) override;

  virtual bool ShouldRestrictToWhitelistedSourceIds() const;

 private:
  friend ::metrics::UkmBrowserTest;
  friend ::metrics::UkmEGTestHelper;
  friend ::ukm::debug::UkmDebugDataExtractor;

  struct MetricAggregate {
    uint64_t total_count = 0;
    double value_sum = 0;
    double value_square_sum = 0.0;
    uint64_t dropped_due_to_limits = 0;
    uint64_t dropped_due_to_sampling = 0;
  };

  struct EventAggregate {
    EventAggregate();
    ~EventAggregate();

    base::flat_map<uint64_t, MetricAggregate> metrics;
    uint64_t total_count = 0;
    uint64_t dropped_due_to_limits = 0;
    uint64_t dropped_due_to_sampling = 0;
  };

  using MetricAggregateMap = std::map<uint64_t, MetricAggregate>;

  void AddEntry(mojom::UkmEntryPtr entry) override;

  // Load sampling configurations from field-trial information.
  void LoadExperimentSamplingInfo();

  // Whether recording new data is currently allowed.
  bool recording_enabled_ = false;

  // Indicates whether recording is enabled for extensions.
  bool extensions_enabled_ = false;

  // Callback for checking extension IDs.
  IsWebstoreExtensionCallback is_webstore_extension_callback_;

  // Contains newly added sources and entries of UKM metrics which periodically
  // get serialized and cleared by StoreRecordingsInReport().
  std::map<SourceId, std::unique_ptr<UkmSource>> sources_;
  std::vector<mojom::UkmEntryPtr> entries_;

  // Whitelisted Entry hashes, only the ones in this set will be recorded.
  std::set<uint64_t> whitelisted_entry_hashes_;

  // Sampling configurations, loaded from a field-trial.
  int default_sampling_rate_ = 0;
  base::flat_map<uint64_t, int> event_sampling_rates_;

  // Aggregate information for collected event metrics.
  std::map<uint64_t, EventAggregate> event_aggregations_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_UKM_RECORDER_IMPL_H_
