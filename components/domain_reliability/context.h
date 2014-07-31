// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_CONTEXT_H_
#define COMPONENTS_DOMAIN_RELIABILITY_CONTEXT_H_

#include <deque>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/domain_reliability/beacon.h"
#include "components/domain_reliability/config.h"
#include "components/domain_reliability/domain_reliability_export.h"
#include "components/domain_reliability/scheduler.h"

class GURL;

namespace base {
class Value;
}

namespace domain_reliability {

class DomainReliabilityDispatcher;
class DomainReliabilityUploader;
class MockableTime;

// The per-domain context for the Domain Reliability client; includes the
// domain's config and per-resource beacon queues.
class DOMAIN_RELIABILITY_EXPORT DomainReliabilityContext {
 public:
  DomainReliabilityContext(
      MockableTime* time,
      const DomainReliabilityScheduler::Params& scheduler_params,
      const std::string& upload_reporter_string,
      DomainReliabilityDispatcher* dispatcher,
      DomainReliabilityUploader* uploader,
      scoped_ptr<const DomainReliabilityConfig> config);
  ~DomainReliabilityContext();

  // Notifies the context of a beacon on its domain(s); may or may not save the
  // actual beacon to be uploaded, depending on the sample rates in the config,
  // but will increment one of the request counters in any case.
  void OnBeacon(const GURL& url, const DomainReliabilityBeacon& beacon);

  // Called to clear browsing data, since beacons are like browsing history.
  void ClearBeacons();

  // Gets a Value containing data that can be formatted into a web page for
  // debugging purposes.
  scoped_ptr<base::Value> GetWebUIData() const;

  void GetQueuedBeaconsForTesting(
      std::vector<DomainReliabilityBeacon>* beacons_out) const;

  void GetRequestCountsForTesting(
      size_t resource_index,
      uint32* successful_requests_out,
      uint32* failed_requests_out) const;

  const DomainReliabilityConfig& config() const { return *config_.get(); }

  // Maximum number of beacons queued per context; if more than this many are
  // queued; the oldest beacons will be removed.
  static const size_t kMaxQueuedBeacons;

 private:
  class ResourceState;

  typedef std::deque<DomainReliabilityBeacon> BeaconDeque;
  typedef ScopedVector<ResourceState> ResourceStateVector;
  typedef ResourceStateVector::const_iterator ResourceStateIterator;

  void InitializeResourceStates();
  void ScheduleUpload(base::TimeDelta min_delay, base::TimeDelta max_delay);
  void StartUpload();
  void OnUploadComplete(bool success);

  scoped_ptr<const base::Value> CreateReport(base::TimeTicks upload_time) const;

  // Remembers the current state of the context when an upload starts. Can be
  // called multiple times in a row (without |CommitUpload|) if uploads fail
  // and are retried.
  void MarkUpload();

  // Uses the state remembered by |MarkUpload| to remove successfully uploaded
  // data but keep beacons and request counts added after the upload started.
  void CommitUpload();

  void RollbackUpload();

  // Finds and removes the oldest beacon. DCHECKs if there is none. (Called
  // when there are too many beacons queued.)
  void RemoveOldestBeacon();

  scoped_ptr<const DomainReliabilityConfig> config_;
  MockableTime* time_;
  const std::string& upload_reporter_string_;
  DomainReliabilityScheduler scheduler_;
  DomainReliabilityDispatcher* dispatcher_;
  DomainReliabilityUploader* uploader_;

  BeaconDeque beacons_;
  size_t uploading_beacons_size_;
  // Each ResourceState in |states_| corresponds to the Resource of the same
  // index in the config.
  ResourceStateVector states_;
  base::TimeTicks upload_time_;
  base::TimeTicks last_upload_time_;

  base::WeakPtrFactory<DomainReliabilityContext> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DomainReliabilityContext);
};

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_CONTEXT_H_
