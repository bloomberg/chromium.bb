// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/quiesce_status_change_checker.h"

#include <stddef.h>

#include "base/format_macros.h"
#include "base/scoped_observer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"

namespace {

// Returns true if this service is disabled.
bool IsSyncDisabled(browser_sync::ProfileSyncService* service) {
  return !service->IsSetupInProgress() && !service->IsFirstSetupComplete();
}

// Returns true if these services have matching progress markers.
bool ProgressMarkersMatch(const browser_sync::ProfileSyncService* service1,
                          const browser_sync::ProfileSyncService* service2) {
  // GetActiveDataTypes() is always empty during configuration, so progress
  // markers cannot be compared.
  if (service1->GetState() != syncer::SyncService::State::ACTIVE ||
      service2->GetState() != syncer::SyncService::State::ACTIVE) {
    return false;
  }

  const syncer::ModelTypeSet common_types =
      Intersection(service1->GetActiveDataTypes(),
                   service2->GetActiveDataTypes());

  const syncer::SyncCycleSnapshot& snap1 = service1->GetLastCycleSnapshot();
  const syncer::SyncCycleSnapshot& snap2 = service2->GetLastCycleSnapshot();

  for (syncer::ModelTypeSet::Iterator type_it = common_types.First();
       type_it.Good(); type_it.Inc()) {
    // Look up the progress markers.  Fail if either one is missing.
    syncer::ProgressMarkerMap::const_iterator pm_it1 =
        snap1.download_progress_markers().find(type_it.Get());
    if (pm_it1 == snap1.download_progress_markers().end()) {
      return false;
    }

    syncer::ProgressMarkerMap::const_iterator pm_it2 =
        snap2.download_progress_markers().find(type_it.Get());
    if (pm_it2 == snap2.download_progress_markers().end()) {
      return false;
    }

    // Fail if any of them don't match.
    if (pm_it1->second != pm_it2->second) {
      return false;
    }
  }
  return true;
}

}  // namespace

// Variation of UpdateProgressMarkerChecker that intercepts calls to
// CheckExitCondition() and forwards them to a parent checker.
class QuiesceStatusChangeChecker::NestedUpdatedProgressMarkerChecker
    : public UpdatedProgressMarkerChecker {
 public:
  NestedUpdatedProgressMarkerChecker(
      browser_sync::ProfileSyncService* service,
      const base::RepeatingClosure& check_exit_condition_cb)
      : UpdatedProgressMarkerChecker(service),
        check_exit_condition_cb_(check_exit_condition_cb) {}

  ~NestedUpdatedProgressMarkerChecker() override {}

 protected:
  void CheckExitCondition() override { check_exit_condition_cb_.Run(); }

 private:
  const base::RepeatingClosure check_exit_condition_cb_;
};

QuiesceStatusChangeChecker::QuiesceStatusChangeChecker(
    std::vector<browser_sync::ProfileSyncService*> services)
    : MultiClientStatusChangeChecker(services) {
  DCHECK_LE(1U, services.size());
  for (size_t i = 0; i < services.size(); ++i) {
    checkers_.push_back(std::make_unique<NestedUpdatedProgressMarkerChecker>(
        services[i],
        base::BindRepeating(&QuiesceStatusChangeChecker::CheckExitCondition,
                            base::Unretained(this))));
  }
}

QuiesceStatusChangeChecker::~QuiesceStatusChangeChecker() {}

bool QuiesceStatusChangeChecker::IsExitConditionSatisfied() {
  // Check that all progress markers are up to date.
  std::vector<browser_sync::ProfileSyncService*> enabled_services;
  for (const auto& checker : checkers_) {
    if (IsSyncDisabled(checker->service())) {
      continue;  // Skip disabled services.
    }

    enabled_services.push_back(checker->service());

    if (!checker->IsExitConditionSatisfied()) {
      DVLOG(1) << "Not quiesced: Progress markers are old.";
      return false;
    }
  }

  for (size_t i = 1; i < enabled_services.size(); ++i) {
    // Return false if there is a progress marker mismatch.
    if (!ProgressMarkersMatch(enabled_services[i - 1], enabled_services[i])) {
      DVLOG(1) << "Not quiesced: Progress marker mismatch.";
      return false;
    }
  }

  return true;
}

std::string QuiesceStatusChangeChecker::GetDebugMessage() const {
  return base::StringPrintf("Waiting for quiescence of %" PRIuS " clients",
                            checkers_.size());
}
