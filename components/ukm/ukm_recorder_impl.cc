// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_recorder_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/string_split.h"
#include "components/ukm/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/metrics_proto/ukm/entry.pb.h"
#include "third_party/metrics_proto/ukm/report.pb.h"
#include "third_party/metrics_proto/ukm/source.pb.h"
#include "url/gurl.h"

namespace ukm {

namespace {

// Gets the list of whitelisted Entries as string. Format is a comma seperated
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

// Gets the maximum number of unferenced Sources kept after purging sources
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
// Note: This currently excludes chrome-extension:// URLs as in order to log
// them, UKM needs to take into account extension-sync consent, which is not
// yet done.
bool HasSupportedScheme(const GURL& url) {
  // Note: kChromeUIScheme is defined in content, which this code can't
  // depend on - since it's used by iOS too. So "chrome" is hardcoded here.
  return url.SchemeIsHTTPOrHTTPS() || url.SchemeIs(url::kFtpScheme) ||
         url.SchemeIs(url::kAboutScheme) || url.SchemeIs("chrome");
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

  UMA_HISTOGRAM_COUNTS_1000("UKM.Sources.SerializedCount",
                            sources_.size() - unsent_sources.size());
  UMA_HISTOGRAM_COUNTS_1000("UKM.Entries.SerializedCount", entries_.size());
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
  sources_.emplace(source_id, base::MakeUnique<UkmSource>(source_id, url));
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
