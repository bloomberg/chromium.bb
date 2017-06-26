// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_UKM_RECORDER_IMPL_H_
#define COMPONENTS_UKM_UKM_RECORDER_IMPL_H_

#include <map>
#include <set>
#include <vector>

#include "base/threading/thread_checker.h"
#include "components/ukm/public/interfaces/ukm_interface.mojom.h"
#include "components/ukm/public/ukm_recorder.h"

namespace metrics {
class UkmBrowserTest;
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

  const std::map<ukm::SourceId, std::unique_ptr<UkmSource>>& sources() const {
    return sources_;
  }

  const std::vector<mojom::UkmEntryPtr>& entries() const { return entries_; }

 private:
  friend ::metrics::UkmBrowserTest;
  friend ::ukm::debug::DebugPage;

  // UkmRecorder:
  void UpdateSourceURL(SourceId source_id, const GURL& url) override;
  void AddEntry(mojom::UkmEntryPtr entry) override;

  // Whether recording new data is currently allowed.
  bool recording_enabled_;

  // Contains newly added sources and entries of UKM metrics which periodically
  // get serialized and cleared by BuildAndStoreLog().
  std::map<ukm::SourceId, std::unique_ptr<UkmSource>> sources_;
  std::vector<mojom::UkmEntryPtr> entries_;

  // Whitelisted Entry hashes, only the ones in this set will be recorded.
  std::set<uint64_t> whitelisted_entry_hashes_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_UKM_RECORDER_IMPL_H_
