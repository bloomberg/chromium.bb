// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/context.h"

#include <algorithm>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/values.h"
#include "components/domain_reliability/beacon.h"
#include "components/domain_reliability/dispatcher.h"
#include "components/domain_reliability/uploader.h"
#include "components/domain_reliability/util.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_context_getter.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;

namespace domain_reliability {

namespace {
typedef std::deque<DomainReliabilityBeacon> BeaconDeque;
typedef BeaconDeque::iterator BeaconIterator;
typedef BeaconDeque::const_iterator BeaconConstIterator;
}  // namespace

class DomainReliabilityContext::ResourceState {
 public:
  ResourceState(DomainReliabilityContext* context,
                const DomainReliabilityConfig::Resource* config)
      : context(context),
        config(config),
        successful_requests(0),
        failed_requests(0),
        uploading_beacons_size(0),
        uploading_successful_requests(0),
        uploading_failed_requests(0) {}
  ~ResourceState() {}

  // Serializes the resource state into a Value to be included in an upload.
  // If there is nothing to report (no beacons and all request counters are 0),
  // returns a scoped_ptr to NULL instead so the resource can be omitted.
  scoped_ptr<base::Value> ToValue(base::TimeTicks upload_time) const {
    if (beacons.empty() && successful_requests == 0 && failed_requests == 0)
      return scoped_ptr<base::Value>();

    ListValue* beacons_value = new ListValue();
    for (BeaconConstIterator it = beacons.begin(); it != beacons.end(); ++it)
      beacons_value->Append(it->ToValue(upload_time));

    DictionaryValue* resource_value = new DictionaryValue();
    resource_value->SetString("resource_name", config->name);
    resource_value->SetInteger("successful_requests", successful_requests);
    resource_value->SetInteger("failed_requests", failed_requests);
    resource_value->Set("beacons", beacons_value);

    return scoped_ptr<Value>(resource_value);
  }

  // Remembers the current state of the resource data when an upload starts.
  void MarkUpload() {
    uploading_beacons_size = beacons.size();
    uploading_successful_requests = successful_requests;
    uploading_failed_requests = failed_requests;
  }

  // Uses the state remembered by |MarkUpload| to remove successfully uploaded
  // data but keep beacons and request counts added after the upload started.
  void CommitUpload() {
    BeaconIterator begin = beacons.begin();
    BeaconIterator end = begin + uploading_beacons_size;
    beacons.erase(begin, end);
    successful_requests -= uploading_successful_requests;
    failed_requests -= uploading_failed_requests;
  }

  // Gets the start time of the oldest beacon, if there are any. Returns true
  // and sets |oldest_start_out| if so; otherwise, returns false.
  bool GetOldestBeaconStart(base::TimeTicks* oldest_start_out) const {
    if (beacons.empty())
      return false;
    *oldest_start_out = beacons[0].start_time;
    return true;
  }

  // Removes the oldest beacon. DCHECKs if there isn't one.
  void RemoveOldestBeacon() {
    DCHECK(!beacons.empty());
    beacons.erase(beacons.begin());
    // If that just removed a beacon counted in uploading_beacons_size,
    // decrement
    // that.
    if (uploading_beacons_size > 0)
      --uploading_beacons_size;
  }

  DomainReliabilityContext* context;
  const DomainReliabilityConfig::Resource* config;

  std::deque<DomainReliabilityBeacon> beacons;
  uint32 successful_requests;
  uint32 failed_requests;

  // State saved during uploads; if an upload succeeds, these are used to
  // remove uploaded data from the beacon list and request counters.
  size_t uploading_beacons_size;
  uint32 uploading_successful_requests;
  uint32 uploading_failed_requests;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceState);
};

// static
const size_t DomainReliabilityContext::kMaxQueuedBeacons = 150;

DomainReliabilityContext::DomainReliabilityContext(
    MockableTime* time,
    const DomainReliabilityScheduler::Params& scheduler_params,
    const std::string& upload_reporter_string,
    DomainReliabilityDispatcher* dispatcher,
    DomainReliabilityUploader* uploader,
    scoped_ptr<const DomainReliabilityConfig> config)
    : config_(config.Pass()),
      time_(time),
      upload_reporter_string_(upload_reporter_string),
      scheduler_(time,
                 config_->collectors.size(),
                 scheduler_params,
                 base::Bind(&DomainReliabilityContext::ScheduleUpload,
                            base::Unretained(this))),
      dispatcher_(dispatcher),
      uploader_(uploader),
      beacon_count_(0),
      uploading_beacon_count_(0),
      weak_factory_(this) {
  InitializeResourceStates();
}

DomainReliabilityContext::~DomainReliabilityContext() {}

void DomainReliabilityContext::OnBeacon(const GURL& url,
                                        const DomainReliabilityBeacon& beacon) {
  size_t index = config_->GetResourceIndexForUrl(url);
  if (index == DomainReliabilityConfig::kInvalidResourceIndex)
    return;
  DCHECK_GT(states_.size(), index);

  bool success = (beacon.status == "ok");

  ResourceState* state = states_[index];
  if (success)
    ++state->successful_requests;
  else
    ++state->failed_requests;

  bool reported = false;
  bool evicted = false;
  if (state->config->DecideIfShouldReportRequest(success)) {
    state->beacons.push_back(beacon);
    ++beacon_count_;
    if (beacon_count_ > kMaxQueuedBeacons) {
      RemoveOldestBeacon();
      evicted = true;
    }
    scheduler_.OnBeaconAdded();
    reported = true;
    UMA_HISTOGRAM_SPARSE_SLOWLY("DomainReliability.ReportedBeaconError",
                                -beacon.chrome_error);
    // TODO(ttuttle): Histogram HTTP response code?
  }

  UMA_HISTOGRAM_BOOLEAN("DomainReliability.BeaconReported", reported);
  UMA_HISTOGRAM_BOOLEAN("DomainReliability.OnBeaconDidEvict", evicted);
}

void DomainReliabilityContext::ClearBeacons() {
  ResourceStateVector::iterator it;
  for (it = states_.begin(); it != states_.end(); ++it) {
    ResourceState* state = *it;
    state->beacons.clear();
    state->successful_requests = 0;
    state->failed_requests = 0;
    state->uploading_beacons_size = 0;
    state->uploading_successful_requests = 0;
    state->uploading_failed_requests = 0;
  }
  beacon_count_ = 0;
  uploading_beacon_count_ = 0;
}

scoped_ptr<base::Value> DomainReliabilityContext::GetWebUIData() const {
  base::DictionaryValue* context_value = new base::DictionaryValue();

  context_value->SetString("domain", config().domain);
  context_value->SetInteger("beacon_count", static_cast<int>(beacon_count_));
  context_value->SetInteger("uploading_beacon_count",
      static_cast<int>(uploading_beacon_count_));
  context_value->Set("scheduler", scheduler_.GetWebUIData());

  return scoped_ptr<base::Value>(context_value);
}

void DomainReliabilityContext::GetQueuedDataForTesting(
    size_t resource_index,
    std::vector<DomainReliabilityBeacon>* beacons_out,
    uint32* successful_requests_out,
    uint32* failed_requests_out) const {
  DCHECK_NE(DomainReliabilityConfig::kInvalidResourceIndex, resource_index);
  DCHECK_GT(states_.size(), resource_index);
  const ResourceState& state = *states_[resource_index];
  if (beacons_out)
    beacons_out->assign(state.beacons.begin(), state.beacons.end());
  if (successful_requests_out)
    *successful_requests_out = state.successful_requests;
  if (failed_requests_out)
    *failed_requests_out = state.failed_requests;
}

void DomainReliabilityContext::InitializeResourceStates() {
  ScopedVector<DomainReliabilityConfig::Resource>::const_iterator it;
  for (it = config_->resources.begin(); it != config_->resources.end(); ++it)
    states_.push_back(new ResourceState(this, *it));
}

void DomainReliabilityContext::ScheduleUpload(
    base::TimeDelta min_delay,
    base::TimeDelta max_delay) {
  dispatcher_->ScheduleTask(
      base::Bind(
          &DomainReliabilityContext::StartUpload,
          weak_factory_.GetWeakPtr()),
      min_delay,
      max_delay);
}

void DomainReliabilityContext::StartUpload() {
  MarkUpload();

  DCHECK(upload_time_.is_null());
  upload_time_ = time_->NowTicks();
  std::string report_json;
  scoped_ptr<const Value> report_value(CreateReport(upload_time_));
  base::JSONWriter::Write(report_value.get(), &report_json);
  report_value.reset();

  size_t collector_index = scheduler_.OnUploadStart();

  uploader_->UploadReport(
      report_json,
      config_->collectors[collector_index]->upload_url,
      base::Bind(
          &DomainReliabilityContext::OnUploadComplete,
          weak_factory_.GetWeakPtr()));

  UMA_HISTOGRAM_BOOLEAN("DomainReliability.UploadFailover",
                        collector_index > 0);
  if (!last_upload_time_.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("DomainReliability.UploadInterval",
                             upload_time_ - last_upload_time_);
  }
}

void DomainReliabilityContext::OnUploadComplete(bool success) {
  if (success)
    CommitUpload();
  scheduler_.OnUploadComplete(success);
  UMA_HISTOGRAM_BOOLEAN("DomainReliability.UploadSuccess", success);
  DCHECK(!upload_time_.is_null());
  UMA_HISTOGRAM_MEDIUM_TIMES("DomainReliability.UploadDuration",
                             time_->NowTicks() - upload_time_);
  last_upload_time_ = upload_time_;
  upload_time_ = base::TimeTicks();
}

scoped_ptr<const Value> DomainReliabilityContext::CreateReport(
    base::TimeTicks upload_time) const {
  ListValue* resources_value = new ListValue();
  for (ResourceStateIterator it = states_.begin(); it != states_.end(); ++it) {
    scoped_ptr<Value> resource_report = (*it)->ToValue(upload_time);
    if (resource_report)
      resources_value->Append(resource_report.release());
  }

  DictionaryValue* report_value = new DictionaryValue();
  report_value->SetString("config_version", config().version);
  report_value->SetString("reporter", upload_reporter_string_);
  report_value->Set("resource_reports", resources_value);

  return scoped_ptr<const Value>(report_value);
}

void DomainReliabilityContext::MarkUpload() {
  for (ResourceStateIterator it = states_.begin(); it != states_.end(); ++it)
    (*it)->MarkUpload();
  uploading_beacon_count_ = beacon_count_;
}

void DomainReliabilityContext::CommitUpload() {
  for (ResourceStateIterator it = states_.begin(); it != states_.end(); ++it)
    (*it)->CommitUpload();
  beacon_count_ -= uploading_beacon_count_;
}

void DomainReliabilityContext::RemoveOldestBeacon() {
  DCHECK_LT(0u, beacon_count_);

  base::TimeTicks min_time;
  ResourceState* min_resource = NULL;
  for (ResourceStateIterator it = states_.begin(); it != states_.end(); ++it) {
    base::TimeTicks oldest;
    if ((*it)->GetOldestBeaconStart(&oldest)) {
      if (!min_resource || oldest < min_time) {
        min_time = oldest;
        min_resource = *it;
      }
    }
  }
  DCHECK(min_resource);

  VLOG(1) << "Beacon queue for " << config().domain << " full; "
          << "removing oldest beacon from " << min_resource->config->name;

  min_resource->RemoveOldestBeacon();
  --beacon_count_;
  // If that just removed a beacon counted in uploading_beacon_count_, decrement
  // that.
  if (uploading_beacon_count_ > 0)
    --uploading_beacon_count_;
}

}  // namespace domain_reliability
