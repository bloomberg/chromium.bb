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
#include "components/domain_reliability/dispatcher.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_context_getter.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;

namespace domain_reliability {

namespace {
const char* kReporter = "chrome";
typedef std::deque<DomainReliabilityBeacon> BeaconDeque;
typedef BeaconDeque::iterator BeaconIterator;
typedef BeaconDeque::const_iterator BeaconConstIterator;
}  // namespace

const int DomainReliabilityContext::kMaxQueuedBeacons = 150;

DomainReliabilityContext::DomainReliabilityContext(
    MockableTime* time,
    const DomainReliabilityScheduler::Params& scheduler_params,
    DomainReliabilityDispatcher* dispatcher,
    DomainReliabilityUploader* uploader,
    scoped_ptr<const DomainReliabilityConfig> config)
    : config_(config.Pass()),
      time_(time),
      scheduler_(time, config_->collectors.size(), scheduler_params,
                 base::Bind(&DomainReliabilityContext::ScheduleUpload,
                            base::Unretained(this))),
      dispatcher_(dispatcher),
      uploader_(uploader),
      beacon_count_(0),
      weak_factory_(this) {
  InitializeResourceStates();
}

DomainReliabilityContext::~DomainReliabilityContext() {}

void DomainReliabilityContext::AddBeacon(
    const DomainReliabilityBeacon& beacon,
    const GURL& url) {
  int index = config_->GetResourceIndexForUrl(url);
  if (index < 0)
    return;
  DCHECK_GT(states_.size(), static_cast<size_t>(index));

  ResourceState* state = states_[index];
  bool success = beacon.http_response_code >= 200 &&
                 beacon.http_response_code < 400;
  if (success)
    ++state->successful_requests;
  else
    ++state->failed_requests;

  VLOG(1) << "Received Beacon: "
          << state->config->name << " "
          << beacon.status << " "
          << beacon.chrome_error << " "
          << beacon.http_response_code << " "
          << beacon.server_ip << " "
          << beacon.elapsed.InMilliseconds() << "ms";

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
  UMA_HISTOGRAM_BOOLEAN("DomainReliability.AddBeaconDidEvict", evicted);
}

void DomainReliabilityContext::GetQueuedDataForTesting(
    int resource_index,
    std::vector<DomainReliabilityBeacon>* beacons_out,
    int* successful_requests_out,
    int* failed_requests_out) const {
  DCHECK_LE(0, resource_index);
  DCHECK_GT(static_cast<int>(states_.size()), resource_index);
  const ResourceState& state = *states_[resource_index];
  if (beacons_out) {
    beacons_out->resize(state.beacons.size());
    std::copy(state.beacons.begin(), state.beacons.end(), beacons_out->begin());
  }
  if (successful_requests_out)
    *successful_requests_out = state.successful_requests;
  if (failed_requests_out)
    *failed_requests_out = state.failed_requests;
}

DomainReliabilityContext::ResourceState::ResourceState(
    DomainReliabilityContext* context,
    const DomainReliabilityConfig::Resource* config)
    : context(context),
      config(config),
      successful_requests(0),
      failed_requests(0) {}

DomainReliabilityContext::ResourceState::~ResourceState() {}

scoped_ptr<Value> DomainReliabilityContext::ResourceState::ToValue(
    base::TimeTicks upload_time) const {
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

void DomainReliabilityContext::ResourceState::MarkUpload() {
  uploading_beacons_size = beacons.size();
  uploading_successful_requests = successful_requests;
  uploading_failed_requests = failed_requests;
}

void DomainReliabilityContext::ResourceState::CommitUpload() {
  BeaconIterator begin = beacons.begin();
  BeaconIterator end = begin + uploading_beacons_size;
  beacons.erase(begin, end);
  successful_requests -= uploading_successful_requests;
  failed_requests -= uploading_failed_requests;
}

bool DomainReliabilityContext::ResourceState::GetOldestBeaconStart(
    base::TimeTicks* oldest_start_out) const {
  if (beacons.empty())
    return false;

  *oldest_start_out = beacons[0].start_time;
  return true;
}

void DomainReliabilityContext::ResourceState::RemoveOldestBeacon() {
  DCHECK(!beacons.empty());
  beacons.erase(beacons.begin());
  // If that just removed a beacon counted in uploading_beacons_size, decrement
  // that.
  if (uploading_beacons_size > 0)
    --uploading_beacons_size;
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

  int collector_index;
  scheduler_.OnUploadStart(&collector_index);

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
  for (ResourceStateIterator it = states_.begin(); it != states_.end(); ++it)
    resources_value->Append((*it)->ToValue(upload_time).release());

  DictionaryValue* report_value = new DictionaryValue();
  report_value->SetString("reporter", kReporter);
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
  DCHECK_LT(0, beacon_count_);

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

  VLOG(1) << "Removing oldest beacon from " << min_resource->config->name;

  min_resource->RemoveOldestBeacon();
  --beacon_count_;
  // If that just removed a beacon counted in uploading_beacon_count_, decrement
  // that.
  if (uploading_beacon_count_ > 0)
    --uploading_beacon_count_;
}

}  // namespace domain_reliability
