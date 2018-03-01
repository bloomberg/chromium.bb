// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_recorder_impl.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "components/ukm/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/metrics_proto/ukm/entry.pb.h"
#include "third_party/metrics_proto/ukm/report.pb.h"
#include "third_party/metrics_proto/ukm/source.pb.h"
#include "url/gurl.h"

namespace ukm {

namespace {

// Note: kChromeUIScheme is defined in content, which this code can't
// depend on - since it's used by iOS too. kExtensionScheme is defined
// in extensions which also isn't always available here.
const char kChromeUIScheme[] = "chrome";
const char kExtensionScheme[] = "chrome-extension";

const base::Feature kUkmSamplingRateFeature{"UkmSamplingRate",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Gets the list of whitelisted Entries as string. Format is a comma separated
// list of Entry names (as strings).
std::string GetWhitelistEntries() {
  return base::GetFieldTrialParamValueByFeature(kUkmFeature,
                                                "WhitelistEntries");
}

bool IsWhitelistedSourceId(SourceId source_id) {
  return GetSourceIdType(source_id) == SourceIdType::NAVIGATION_ID;
}

// Gets the maximum number of Sources we'll keep in memory before discarding any
// new ones being added.
size_t GetMaxSources() {
  constexpr size_t kDefaultMaxSources = 500;
  return static_cast<size_t>(base::GetFieldTrialParamByFeatureAsInt(
      kUkmFeature, "MaxSources", kDefaultMaxSources));
}

// Gets the maximum number of unreferenced Sources kept after purging sources
// that were added to the log.
size_t GetMaxKeptSources() {
  constexpr size_t kDefaultMaxKeptSources = 100;
  return static_cast<size_t>(base::GetFieldTrialParamByFeatureAsInt(
      kUkmFeature, "MaxKeptSources", kDefaultMaxKeptSources));
}

// Gets the maximum number of Entries we'll keep in memory before discarding any
// new ones being added.
size_t GetMaxEntries() {
  constexpr size_t kDefaultMaxEntries = 5000;
  return static_cast<size_t>(base::GetFieldTrialParamByFeatureAsInt(
      kUkmFeature, "MaxEntries", kDefaultMaxEntries));
}

// Returns whether |url| has one of the schemes supported for logging to UKM.
// URLs with other schemes will not be logged.
bool HasSupportedScheme(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS() || url.SchemeIs(url::kFtpScheme) ||
         url.SchemeIs(url::kAboutScheme) || url.SchemeIs(kChromeUIScheme) ||
         url.SchemeIs(kExtensionScheme);
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
  UNSUPPORTED_URL_SCHEME = 4,
  SAMPLED_OUT = 5,
  EXTENSION_URLS_DISABLED = 6,
  EXTENSION_NOT_SYNCED = 7,
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

GURL SanitizeURL(const GURL& url) {
  GURL::Replacements remove_params;
  remove_params.ClearUsername();
  remove_params.ClearPassword();
  // chrome:// and about: URLs params are never used for navigation, only to
  // prepopulate data on the page, so don't include their params.
  if (url.SchemeIs(url::kAboutScheme) || url.SchemeIs("chrome")) {
    remove_params.ClearQuery();
  }
  return url.ReplaceComponents(remove_params);
}

}  // namespace

UkmRecorderImpl::UkmRecorderImpl() : recording_enabled_(false) {}
UkmRecorderImpl::~UkmRecorderImpl() = default;

void UkmRecorderImpl::EnableRecording(bool extensions) {
  DVLOG(1) << "UkmRecorderImpl::EnableRecording, extensions=" << extensions;
  recording_enabled_ = true;
  extensions_enabled_ = extensions;
}

void UkmRecorderImpl::DisableRecording() {
  DVLOG(1) << "UkmRecorderImpl::DisableRecording";
  recording_enabled_ = false;
  extensions_enabled_ = false;
}

void UkmRecorderImpl::Purge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sources_.clear();
  entries_.clear();
}

void UkmRecorderImpl::SetIsWebstoreExtensionCallback(
    const IsWebstoreExtensionCallback& callback) {
  is_webstore_extension_callback_ = callback;
}

void UkmRecorderImpl::StoreRecordingsInReport(Report* report) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::set<SourceId> ids_seen;
  for (const auto& entry : entries_) {
    Entry* proto_entry = report->add_entries();
    StoreEntryProto(*entry, proto_entry);
    ids_seen.insert(entry->source_id);
  }

  std::vector<std::unique_ptr<UkmSource>> unsent_sources;
  for (auto& kv : sources_) {
    // If the source id is not whitelisted, don't send it unless it has
    // associated entries. Note: If ShouldRestrictToWhitelistedSourceIds() is
    // true, this logic will not be hit as the source would have already been
    // filtered in UpdateSourceURL().
    if (!IsWhitelistedSourceId(kv.first) &&
        !base::ContainsKey(ids_seen, kv.first)) {
      unsent_sources.push_back(std::move(kv.second));
      continue;
    }
    Source* proto_source = report->add_sources();
    kv.second->PopulateProto(proto_source);
    if (!ShouldRecordInitialUrl())
      proto_source->clear_initial_url();
  }
  for (const auto& event_and_aggregate : event_aggregations_) {
    if (event_and_aggregate.second.metrics.empty())
      continue;
    const EventAggregate& event_aggregate = event_and_aggregate.second;
    Aggregate* proto_aggregate = report->add_aggregates();
    proto_aggregate->set_source_id(0);  // Across all sources.
    proto_aggregate->set_event_hash(event_and_aggregate.first);
    proto_aggregate->set_total_count(event_aggregate.total_count);
    proto_aggregate->set_dropped_due_to_limits(
        event_aggregate.dropped_due_to_limits);
    proto_aggregate->set_dropped_due_to_sampling(
        event_aggregate.dropped_due_to_sampling);
    for (const auto& metric_and_aggregate : event_aggregate.metrics) {
      const MetricAggregate& aggregate = metric_and_aggregate.second;
      Aggregate::Metric* proto_metric = proto_aggregate->add_metrics();
      proto_metric->set_metric_hash(metric_and_aggregate.first);
      proto_metric->set_value_sum(aggregate.value_sum);
      proto_metric->set_value_square_sum(aggregate.value_square_sum);
      if (aggregate.total_count != event_aggregate.total_count) {
        proto_metric->set_total_count(aggregate.total_count);
      }
      if (aggregate.dropped_due_to_limits !=
          event_aggregate.dropped_due_to_limits) {
        proto_metric->set_dropped_due_to_limits(
            aggregate.dropped_due_to_limits);
      }
      if (aggregate.dropped_due_to_sampling !=
          event_aggregate.dropped_due_to_sampling) {
        proto_metric->set_dropped_due_to_sampling(
            aggregate.dropped_due_to_sampling);
      }
    }
  }

  UMA_HISTOGRAM_COUNTS_1000("UKM.Sources.SerializedCount",
                            sources_.size() - unsent_sources.size());
  UMA_HISTOGRAM_COUNTS_100000("UKM.Entries.SerializedCount2", entries_.size());
  UMA_HISTOGRAM_COUNTS_1000("UKM.Sources.UnsentSourcesCount",
                            unsent_sources.size());
  sources_.clear();
  entries_.clear();

  // Keep at most |max_kept_sources|, prioritizing most-recent entries (by
  // creation time).
  const size_t max_kept_sources = GetMaxKeptSources();
  if (unsent_sources.size() > max_kept_sources) {
    std::nth_element(unsent_sources.begin(),
                     unsent_sources.begin() + max_kept_sources,
                     unsent_sources.end(),
                     [](const std::unique_ptr<ukm::UkmSource>& lhs,
                        const std::unique_ptr<ukm::UkmSource>& rhs) {
                       return lhs->creation_time() > rhs->creation_time();
                     });
    unsent_sources.resize(max_kept_sources);
  }

  for (auto& source : unsent_sources) {
    sources_.emplace(source->id(), std::move(source));
  }
  UMA_HISTOGRAM_COUNTS_1000("UKM.Sources.KeptSourcesCount", sources_.size());
}

bool UkmRecorderImpl::ShouldRestrictToWhitelistedSourceIds() const {
  return base::GetFieldTrialParamByFeatureAsBool(
      kUkmFeature, "RestrictToWhitelistedSourceIds", false);
}

UkmRecorderImpl::EventAggregate::EventAggregate() = default;
UkmRecorderImpl::EventAggregate::~EventAggregate() = default;

void UkmRecorderImpl::UpdateSourceURL(SourceId source_id,
                                      const GURL& unsanitized_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!recording_enabled_) {
    RecordDroppedSource(DroppedDataReason::RECORDING_DISABLED);
    return;
  }

  if (ShouldRestrictToWhitelistedSourceIds() &&
      !IsWhitelistedSourceId(source_id)) {
    RecordDroppedSource(DroppedDataReason::NOT_WHITELISTED);
    return;
  }

  GURL url = SanitizeURL(unsanitized_url);

  if (!HasSupportedScheme(url)) {
    RecordDroppedSource(DroppedDataReason::UNSUPPORTED_URL_SCHEME);
    return;
  }

  // Extension URLs need to be specifically enabled and the extension synced.
  if (url.SchemeIs(kExtensionScheme)) {
    if (!extensions_enabled_) {
      RecordDroppedSource(DroppedDataReason::EXTENSION_URLS_DISABLED);
      return;
    }
    if (!is_webstore_extension_callback_ ||
        !is_webstore_extension_callback_.Run(url.host_piece())) {
      RecordDroppedSource(DroppedDataReason::EXTENSION_NOT_SYNCED);
      return;
    }
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
  sources_.emplace(source_id, std::make_unique<UkmSource>(source_id, url));
}

void UkmRecorderImpl::AddEntry(mojom::UkmEntryPtr entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!recording_enabled_) {
    RecordDroppedEntry(DroppedDataReason::RECORDING_DISABLED);
    return;
  }

  if (!whitelisted_entry_hashes_.empty() &&
      !base::ContainsKey(whitelisted_entry_hashes_, entry->event_hash)) {
    RecordDroppedEntry(DroppedDataReason::NOT_WHITELISTED);
    return;
  }

  if (default_sampling_rate_ == 0)
    LoadExperimentSamplingInfo();

  EventAggregate& event_aggregate = event_aggregations_[entry->event_hash];
  event_aggregate.total_count++;
  for (const auto& metric : entry->metrics) {
    MetricAggregate& aggregate = event_aggregate.metrics[metric->metric_hash];
    double value = metric->value;
    aggregate.total_count++;
    aggregate.value_sum += value;
    aggregate.value_square_sum += value * value;
  }

  auto found = event_sampling_rates_.find(entry->event_hash);
  int sampling_rate = (found != event_sampling_rates_.end())
                          ? found->second
                          : default_sampling_rate_;
  if (sampling_rate == 0 ||
      (sampling_rate > 1 && base::RandInt(1, sampling_rate) != 1)) {
    RecordDroppedEntry(DroppedDataReason::SAMPLED_OUT);
    event_aggregate.dropped_due_to_sampling++;
    for (auto& metric : entry->metrics)
      event_aggregate.metrics[metric->metric_hash].dropped_due_to_sampling++;
    return;
  }

  if (entries_.size() >= GetMaxEntries()) {
    RecordDroppedEntry(DroppedDataReason::MAX_HIT);
    event_aggregate.dropped_due_to_limits++;
    for (auto& metric : entry->metrics)
      event_aggregate.metrics[metric->metric_hash].dropped_due_to_limits++;
    return;
  }

  entries_.push_back(std::move(entry));
}

void UkmRecorderImpl::LoadExperimentSamplingInfo() {
  DCHECK_EQ(0, default_sampling_rate_);
  std::map<std::string, std::string> params;

  if (base::FeatureList::IsEnabled(kUkmSamplingRateFeature)) {
    // Enabled may have various parameters to control sampling.
    if (base::GetFieldTrialParamsByFeature(kUkmSamplingRateFeature, &params)) {
      for (const auto& kv : params) {
        const std::string& key = kv.first;
        if (key.length() == 0)
          continue;

        // Keys starting with an underscore are global configuration.
        if (key.at(0) == '_') {
          if (key == "_default_sampling") {
            int sampling;
            if (base::StringToInt(kv.second, &sampling) && sampling >= 0)
              default_sampling_rate_ = sampling;
          }
          continue;
        }

        // Anything else is an event name.
        int sampling;
        if (base::StringToInt(kv.second, &sampling) && sampling >= 0)
          event_sampling_rates_[base::HashMetricName(key)] = sampling;
      }
    }
  }

  // Default rate must be >0 to indicate that load is complete.
  if (default_sampling_rate_ == 0)
    default_sampling_rate_ = 1;
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
