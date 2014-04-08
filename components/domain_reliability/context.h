// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_CONTEXT_H_
#define COMPONENTS_DOMAIN_RELIABILITY_CONTEXT_H_

#include <deque>
#include <map>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/domain_reliability/beacon.h"
#include "components/domain_reliability/config.h"
#include "components/domain_reliability/domain_reliability_export.h"
#include "components/domain_reliability/scheduler.h"
#include "components/domain_reliability/uploader.h"
#include "components/domain_reliability/util.h"

class GURL;

namespace domain_reliability {

class DomainReliabilityDispatcher;
class MockableTime;

// The per-domain context for the Domain Reliability client; includes the
// domain's config and per-resource beacon queues.
class DOMAIN_RELIABILITY_EXPORT DomainReliabilityContext {
 public:
  DomainReliabilityContext(
      MockableTime* time,
      const DomainReliabilityScheduler::Params& scheduler_params,
      DomainReliabilityDispatcher* dispatcher,
      DomainReliabilityUploader* uploader,
      scoped_ptr<const DomainReliabilityConfig> config);
  virtual ~DomainReliabilityContext();

  void AddBeacon(const DomainReliabilityBeacon& beacon, const GURL& url);

  void GetQueuedDataForTesting(
      int resource_index,
      std::vector<DomainReliabilityBeacon>* beacons_out,
      int* successful_requests_out,
      int* failed_requests_out) const;

  const DomainReliabilityConfig& config() { return *config_.get(); }

  // Maximum number of beacons queued per context; if more than this many are
  // queued; the oldest beacons will be removed.
  static const int kMaxQueuedBeacons;

 private:
  // Resource-specific state (queued beacons and request counts).
  class ResourceState {
   public:
    ResourceState(DomainReliabilityContext* context,
                  const DomainReliabilityConfig::Resource* config);
    ~ResourceState();

    scoped_ptr<base::Value> ToValue(base::TimeTicks upload_time) const;

    // Remembers the current state of the resource data when an upload starts.
    void MarkUpload();

    // Uses the state remembered by |MarkUpload| to remove successfully uploaded
    // data but keep beacons and request counts added after the upload started.
    void CommitUpload();

    // Gets the start time of the oldest beacon, if there are any. Returns true
    // and sets |oldest_start_out| if so; otherwise, returns false.
    bool GetOldestBeaconStart(base::TimeTicks* oldest_start_out) const;
    // Removes the oldest beacon. DCHECKs if there isn't one.
    void RemoveOldestBeacon();

    DomainReliabilityContext* context;
    const DomainReliabilityConfig::Resource* config;

    std::deque<DomainReliabilityBeacon> beacons;
    int successful_requests;
    int failed_requests;

    // State saved during uploads; if an upload succeeds, these are used to
    // remove uploaded data from the beacon list and request counters.
    size_t uploading_beacons_size;
    int uploading_successful_requests;
    int uploading_failed_requests;

   private:
    DISALLOW_COPY_AND_ASSIGN(ResourceState);
  };

  typedef ScopedVector<ResourceState> ResourceStateVector;
  typedef ResourceStateVector::const_iterator ResourceStateIterator;

  void InitializeResourceStates();
  void ScheduleUpload(base::TimeDelta min_delay, base::TimeDelta max_delay);
  void StartUpload();
  void OnUploadComplete(bool success);

  scoped_ptr<const base::Value> CreateReport(base::TimeTicks upload_time) const;

  // Remembers the current state of the context when an upload starts.
  void MarkUpload();

  // Uses the state remembered by |MarkUpload| to remove successfully uploaded
  // data but keep beacons and request counts added after the upload started.
  void CommitUpload();

  // Finds and removes the oldest beacon. DCHECKs if there is none. (Called
  // when there are too many beacons queued.)
  void RemoveOldestBeacon();

  scoped_ptr<const DomainReliabilityConfig> config_;
  MockableTime* time_;
  DomainReliabilityScheduler scheduler_;
  DomainReliabilityDispatcher* dispatcher_;
  DomainReliabilityUploader* uploader_;

  // Each ResourceState in |states_| corresponds to the Resource of the same
  // index in the config.
  ResourceStateVector states_;
  int beacon_count_;
  int uploading_beacon_count_;
  base::TimeTicks upload_time_;
  base::TimeTicks last_upload_time_;

  base::WeakPtrFactory<DomainReliabilityContext> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DomainReliabilityContext);
};

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_CONTEXT_H_
