// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_recorder_impl.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/string_split.h"
#include "components/ukm/ukm_source.h"
#include "third_party/metrics_proto/ukm/entry.pb.h"
#include "third_party/metrics_proto/ukm/report.pb.h"
#include "third_party/metrics_proto/ukm/source.pb.h"

namespace ukm {

namespace {

// Gets the list of whitelisted Entries as string. Format is a comma seperated
// list of Entry names (as strings).
std::string GetWhitelistEntries() {
  return base::GetFieldTrialParamValueByFeature(kUkmFeature,
                                                "WhitelistEntries");
}

// Gets the maximum number of Sources we'll keep in memory before discarding any
// new ones being added.
size_t GetMaxSources() {
  constexpr size_t kDefaultMaxSources = 500;
  return static_cast<size_t>(base::GetFieldTrialParamByFeatureAsInt(
      kUkmFeature, "MaxSources", kDefaultMaxSources));
}

// Gets the maximum number of Entries we'll keep in memory before discarding any
// new ones being added.
size_t GetMaxEntries() {
  constexpr size_t kDefaultMaxEntries = 5000;
  return static_cast<size_t>(base::GetFieldTrialParamByFeatureAsInt(
      kUkmFeature, "MaxEntries", kDefaultMaxEntries));
}

// True if we should record the initial_url field of the UKM Source proto.
bool ShouldRecordInitialUrl() {
  return base::GetFieldTrialParamByFeatureAsBool(kUkmFeature,
                                                 "RecordInitialUrl", false);
}

enum class DroppedDataReason {
  NOT_DROPPED = 0,
  RECORDING_DISABLED = 1,
  MAX_HIT = 2,
  NOT_WHITELISTED = 3,
  NUM_DROPPED_DATA_REASONS
};

void RecordDroppedSource(DroppedDataReason reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "UKM.Sources.Dropped", static_cast<int>(reason),
      static_cast<int>(DroppedDataReason::NUM_DROPPED_DATA_REASONS));
}

void RecordDroppedEntry(DroppedDataReason reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "UKM.Entries.Dropped", static_cast<int>(reason),
      static_cast<int>(DroppedDataReason::NUM_DROPPED_DATA_REASONS));
}

void StoreEntryProto(const mojom::UkmEntry& in, Entry* out) {
  DCHECK(!out->has_source_id());
  DCHECK(!out->has_event_hash());

  out->set_source_id(in.source_id);
  out->set_event_hash(in.event_hash);
  for (const auto& metric : in.metrics) {
    Entry::Metric* proto_metric = out->add_metrics();
    proto_metric->set_metric_hash(metric->metric_hash);
    proto_metric->set_value(metric->value);
  }
}

}  // namespace

UkmRecorderImpl::UkmRecorderImpl() : recording_enabled_(false) {}
UkmRecorderImpl::~UkmRecorderImpl() = default;

void UkmRecorderImpl::EnableRecording() {
  DVLOG(1) << "UkmRecorderImpl::EnableRecording";
  recording_enabled_ = true;
}

void UkmRecorderImpl::DisableRecording() {
  DVLOG(1) << "UkmRecorderImpl::DisableRecording";
  recording_enabled_ = false;
}

void UkmRecorderImpl::Purge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sources_.clear();
  entries_.clear();
}

void UkmRecorderImpl::StoreRecordingsInReport(Report* report) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& kv : sources_) {
    Source* proto_source = report->add_sources();
    kv.second->PopulateProto(proto_source);
    if (!ShouldRecordInitialUrl())
      proto_source->clear_initial_url();
  }
  for (const auto& entry : entries_) {
    Entry* proto_entry = report->add_entries();
    StoreEntryProto(*entry, proto_entry);
  }

  UMA_HISTOGRAM_COUNTS_1000("UKM.Sources.SerializedCount", sources_.size());
  UMA_HISTOGRAM_COUNTS_1000("UKM.Entries.SerializedCount", entries_.size());
  sources_.clear();
  entries_.clear();
}

void UkmRecorderImpl::UpdateSourceURL(ukm::SourceId source_id,
                                      const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!recording_enabled_) {
    RecordDroppedSource(DroppedDataReason::RECORDING_DISABLED);
    return;
  }

  // Update the pre-existing source if there is any. This happens when the
  // initial URL is different from the committed URL for the same source, e.g.,
  // when there is redirection.
  if (base::ContainsKey(sources_, source_id)) {
    sources_[source_id]->UpdateUrl(url);
    return;
  }

  if (sources_.size() >= GetMaxSources()) {
    RecordDroppedSource(DroppedDataReason::MAX_HIT);
    return;
  }
  std::unique_ptr<UkmSource> source = base::MakeUnique<UkmSource>();
  source->set_id(source_id);
  source->set_url(url);
  sources_.insert(std::make_pair(source_id, std::move(source)));
}

void UkmRecorderImpl::AddEntry(mojom::UkmEntryPtr entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!recording_enabled_) {
    RecordDroppedEntry(DroppedDataReason::RECORDING_DISABLED);
    return;
  }
  if (entries_.size() >= GetMaxEntries()) {
    RecordDroppedEntry(DroppedDataReason::MAX_HIT);
    return;
  }

  if (!whitelisted_entry_hashes_.empty() &&
      !base::ContainsKey(whitelisted_entry_hashes_, entry->event_hash)) {
    RecordDroppedEntry(DroppedDataReason::NOT_WHITELISTED);
    return;
  }

  entries_.push_back(std::move(entry));
}

void UkmRecorderImpl::StoreWhitelistedEntries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto entries =
      base::SplitString(GetWhitelistEntries(), ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  for (const auto& entry_string : entries)
    whitelisted_entry_hashes_.insert(base::HashMetricName(entry_string));
}

}  // namespace ukm
