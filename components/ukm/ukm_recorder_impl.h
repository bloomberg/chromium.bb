// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_UKM_RECORDER_IMPL_H_
#define COMPONENTS_UKM_UKM_RECORDER_IMPL_H_

#include <map>
#include <set>
#include <vector>

#include "base/sequence_checker.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/interfaces/ukm_interface.mojom.h"

namespace metrics {
class UkmBrowserTest;
class UkmEGTestHelper;
}

namespace ukm {

class UkmSource;
class Report;

namespace debug {
class DebugPage;
}

class UkmRecorderImpl : public UkmRecorder {
 public:
  UkmRecorderImpl();
  ~UkmRecorderImpl() override;

  // Enables/disables recording control if data is allowed to be collected.
  void EnableRecording();
  void DisableRecording();

  // Deletes stored recordings.
  void Purge();

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
  friend ::ukm::debug::DebugPage;

  void AddEntry(mojom::UkmEntryPtr entry) override;

  // Whether recording new data is currently allowed.
  bool recording_enabled_;

  // Contains newly added sources and entries of UKM metrics which periodically
  // get serialized and cleared by StoreRecordingsInReport().
  std::map<SourceId, std::unique_ptr<UkmSource>> sources_;
  std::vector<mojom::UkmEntryPtr> entries_;

  // Whitelisted Entry hashes, only the ones in this set will be recorded.
  std::set<uint64_t> whitelisted_entry_hashes_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_UKM_RECORDER_IMPL_H_
