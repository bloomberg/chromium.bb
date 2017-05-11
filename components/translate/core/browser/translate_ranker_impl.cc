// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_ranker_impl.h"

#include <cmath>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/metrics/proto/translate_event.pb.h"
#include "components/translate/core/browser/proto/ranker_model.pb.h"
#include "components/translate/core/browser/proto/translate_ranker_model.pb.h"
#include "components/translate/core/browser/ranker_model.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_url_fetcher.h"
#include "components/translate/core/common/translate_switches.h"
#include "components/ukm/ukm_entry_builder.h"
#include "components/ukm/ukm_service.h"
#include "components/variations/variations_associated_data.h"
#include "url/gurl.h"

namespace translate {

namespace {

using chrome_intelligence::RankerModel;
using chrome_intelligence::RankerModelProto;
using chrome_intelligence::TranslateRankerModel;

const double kTranslationOfferThreshold = 0.5;

const char kTranslateRankerModelFileName[] = "Translate Ranker Model";
const char kUmaPrefix[] = "Translate.Ranker";
const char kUnknown[] = "UNKNOWN";

double Sigmoid(double x) {
  return 1.0 / (1.0 + exp(-x));
}

double SafeRatio(int numerator, int denominator) {
  return denominator ? (numerator / static_cast<double>(denominator)) : 0.0;
}

double ScoreComponent(const google::protobuf::Map<std::string, float>& weights,
                      const std::string& key) {
  auto i = weights.find(base::ToLowerASCII(key));
  if (i == weights.end())
    i = weights.find(kUnknown);
  return i == weights.end() ? 0.0 : i->second;
}

RankerModelStatus ValidateModel(const RankerModel& model) {
  if (model.proto().model_case() != RankerModelProto::kTranslate)
    return RankerModelStatus::VALIDATION_FAILED;

  if (model.proto().translate().model_revision_case() !=
      TranslateRankerModel::kLogisticRegressionModel) {
    return RankerModelStatus::INCOMPATIBLE;
  }

  return RankerModelStatus::OK;
}

}  // namespace

const base::Feature kTranslateRankerQuery{"TranslateRankerQuery",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTranslateRankerEnforcement{
    "TranslateRankerEnforcement", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTranslateRankerDecisionOverride{
    "TranslateRankerDecisionOverride", base::FEATURE_DISABLED_BY_DEFAULT};

TranslateRankerFeatures::TranslateRankerFeatures() {}

TranslateRankerFeatures::TranslateRankerFeatures(int accepted,
                                                 int denied,
                                                 int ignored,
                                                 const std::string& src,
                                                 const std::string& dst,
                                                 const std::string& cntry,
                                                 const std::string& locale)
    : accepted_count(accepted),
      denied_count(denied),
      ignored_count(ignored),
      total_count(accepted_count + denied_count + ignored_count),
      src_lang(src),
      dst_lang(dst),
      country(cntry),
      app_locale(locale),
      accepted_ratio(SafeRatio(accepted_count, total_count)),
      denied_ratio(SafeRatio(denied_count, total_count)),
      ignored_ratio(SafeRatio(ignored_count, total_count)) {}

TranslateRankerFeatures::TranslateRankerFeatures(const TranslatePrefs& prefs,
                                                 const std::string& src,
                                                 const std::string& dst,
                                                 const std::string& locale)
    : TranslateRankerFeatures(prefs.GetTranslationAcceptedCount(src),
                              prefs.GetTranslationDeniedCount(src),
                              prefs.GetTranslationIgnoredCount(src),
                              src,
                              dst,
                              prefs.GetCountry(),
                              locale) {}

TranslateRankerFeatures::~TranslateRankerFeatures() {}

void TranslateRankerFeatures::WriteTo(std::ostream& stream) const {
  stream << "src_lang='" << src_lang << "', "
         << "dst_lang='" << dst_lang << "', "
         << "country='" << country << "', "
         << "app_locale='" << app_locale << "', "
         << "accept_count=" << accepted_count << ", "
         << "denied_count=" << denied_count << ", "
         << "ignored_count=" << ignored_count << ", "
         << "total_count=" << total_count << ", "
         << "accept_ratio=" << accepted_ratio << ", "
         << "decline_ratio=" << denied_ratio << ", "
         << "ignore_ratio=" << ignored_ratio;
}

TranslateRankerImpl::TranslateRankerImpl(const base::FilePath& model_path,
                                         const GURL& model_url,
                                         ukm::UkmService* ukm_service)
    : ukm_service_(ukm_service),
      is_logging_enabled_(true),
      is_query_enabled_(base::FeatureList::IsEnabled(kTranslateRankerQuery)),
      is_enforcement_enabled_(
          base::FeatureList::IsEnabled(kTranslateRankerEnforcement)),
      is_decision_override_enabled_(base::FeatureList::IsEnabled(
          translate::kTranslateRankerDecisionOverride)),
      weak_ptr_factory_(this) {
  if (is_query_enabled_ || is_enforcement_enabled_ ||
      is_decision_override_enabled_) {
    model_loader_ = base::MakeUnique<RankerModelLoader>(
        base::Bind(&ValidateModel),
        base::Bind(&TranslateRankerImpl::OnModelAvailable,
                   weak_ptr_factory_.GetWeakPtr()),
        model_path, model_url, kUmaPrefix);
  }
}

TranslateRankerImpl::~TranslateRankerImpl() {}

// static
base::FilePath TranslateRankerImpl::GetModelPath(
    const base::FilePath& data_dir) {
  if (data_dir.empty())
    return base::FilePath();

  // Otherwise, look for the file in data dir.
  return data_dir.AppendASCII(kTranslateRankerModelFileName);
}

// static
GURL TranslateRankerImpl::GetModelURL() {
  // Allow override of the ranker model URL from the command line.
  std::string raw_url;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kTranslateRankerModelURL)) {
    raw_url =
        command_line->GetSwitchValueASCII(switches::kTranslateRankerModelURL);
  } else {
    // Otherwise take the ranker model URL from the ranker query variation.
    raw_url = variations::GetVariationParamValueByFeature(
        kTranslateRankerQuery, switches::kTranslateRankerModelURL);
  }

  DVLOG(3) << switches::kTranslateRankerModelURL << " = " << raw_url;

  return GURL(raw_url);
}

void TranslateRankerImpl::EnableLogging(bool value) {
  is_logging_enabled_ = value;
}

uint32_t TranslateRankerImpl::GetModelVersion() const {
  return model_ ? model_->proto().translate().version() : 0;
}

bool TranslateRankerImpl::ShouldOfferTranslation(
    const TranslatePrefs& translate_prefs,
    const std::string& src_lang,
    const std::string& dst_lang,
    metrics::TranslateEventProto* translate_event) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  // The ranker is a gate in the "show a translation prompt" flow. To retain
  // the pre-existing functionality, it defaults to returning true in the
  // absence of a model or if enforcement is disabled. As this is ranker is
  // subsumed into a more general assist ranker, this default will go away
  // (or become False).
  const bool kDefaultResponse = true;

  translate_event->set_ranker_request_timestamp_sec(
      (base::TimeTicks::Now() - base::TimeTicks()).InSeconds());
  translate_event->set_ranker_version(GetModelVersion());

  if (!is_query_enabled_ && !is_enforcement_enabled_ &&
      !is_decision_override_enabled_) {
    translate_event->set_ranker_response(
        metrics::TranslateEventProto::NOT_QUERIED);
    return kDefaultResponse;
  }

  if (model_loader_)
    model_loader_->NotifyOfRankerActivity();

  // If we don't have a model, request one and return the default.
  if (model_ == nullptr) {
    translate_event->set_ranker_response(
        metrics::TranslateEventProto::NOT_QUERIED);
    return kDefaultResponse;
  }

  SCOPED_UMA_HISTOGRAM_TIMER("Translate.Ranker.Timer.ShouldOfferTranslation");

  // TODO(rogerm): Remove ScopedTracker below once crbug.com/646711 is closed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "646711 translate::TranslateRankerImpl::ShouldOfferTranslation"));

  TranslateRankerFeatures features(
      translate_prefs, src_lang, dst_lang,
      TranslateDownloadManager::GetInstance()->application_locale());

  double score = CalculateScore(features);

  DVLOG(2) << "TranslateRankerImpl::ShouldOfferTranslation: "
           << "Score = " << score << ", Features=[" << features << "]";

  bool result = (score >= kTranslationOfferThreshold);

  UMA_HISTOGRAM_BOOLEAN("Translate.Ranker.QueryResult", result);

  translate_event->set_ranker_response(
      result ? metrics::TranslateEventProto::SHOW
             : metrics::TranslateEventProto::DONT_SHOW);

  if (!is_enforcement_enabled_ && !is_decision_override_enabled_) {
    return kDefaultResponse;
  }

  return result;
}

double TranslateRankerImpl::CalculateScore(
    const TranslateRankerFeatures& features) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  SCOPED_UMA_HISTOGRAM_TIMER("Translate.Ranker.Timer.CalculateScore");
  DCHECK(model_ != nullptr);
  const TranslateRankerModel::LogisticRegressionModel& logit =
      model_->proto().translate().logistic_regression_model();

  double dot_product =
      (features.accepted_count * logit.accept_count_weight()) +
      (features.denied_count * logit.decline_count_weight()) +
      (features.ignored_count * logit.ignore_count_weight()) +
      (features.accepted_ratio * logit.accept_ratio_weight()) +
      (features.denied_ratio * logit.decline_ratio_weight()) +
      (features.ignored_ratio * logit.ignore_ratio_weight()) +
      ScoreComponent(logit.source_language_weight(), features.src_lang) +
      ScoreComponent(logit.target_language_weight(), features.dst_lang) +
      ScoreComponent(logit.country_weight(), features.country) +
      ScoreComponent(logit.locale_weight(), features.app_locale);

  return Sigmoid(dot_product + logit.bias());
}

void TranslateRankerImpl::FlushTranslateEvents(
    std::vector<metrics::TranslateEventProto>* events) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DVLOG(3) << "Flushing translate ranker events.";
  events->swap(event_cache_);
  event_cache_.clear();
}

void TranslateRankerImpl::SendEventToUKM(
    const metrics::TranslateEventProto& event,
    const GURL& url) {
  if (!ukm_service_) {
    DVLOG(3) << "No UKM service.";
    return;
  }
  DVLOG(3) << "Sending event for url: " << url.spec();
  int32_t source_id = ukm_service_->GetNewSourceID();
  ukm_service_->UpdateSourceURL(source_id, url);
  std::unique_ptr<ukm::UkmEntryBuilder> builder =
      ukm_service_->GetEntryBuilder(source_id, "Translate");
  // The metrics added here should be kept in sync with the documented
  // metrics in tools/metrics/ukm/ukm.xml.
  // TODO(hamelphi): Remove hashing functions once UKM accepts strings metrics.
  builder->AddMetric("SourceLanguage",
                     base::HashMetricName(event.source_language()));
  builder->AddMetric("TargetLanguage",
                     base::HashMetricName(event.target_language()));
  builder->AddMetric("Country", base::HashMetricName(event.country()));
  builder->AddMetric("AcceptCount", event.accept_count());
  builder->AddMetric("DeclineCount", event.decline_count());
  builder->AddMetric("IgnoreCount", event.ignore_count());
  builder->AddMetric("RankerVersion", event.ranker_version());
  builder->AddMetric("RankerResponse", event.ranker_response());
  builder->AddMetric("EventType", event.event_type());
}

void TranslateRankerImpl::AddTranslateEvent(
    const metrics::TranslateEventProto& event,
    const GURL& url) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  if (is_logging_enabled_) {
    DVLOG(3) << "Adding translate ranker event.";
    if (url.is_valid()) {
      SendEventToUKM(event, url);
    }
    event_cache_.push_back(event);
  }
}

void TranslateRankerImpl::OnModelAvailable(std::unique_ptr<RankerModel> model) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  model_ = std::move(model);
}

bool TranslateRankerImpl::CheckModelLoaderForTesting() {
  return model_loader_ != nullptr;
}

void TranslateRankerImpl::RecordTranslateEvent(
    int event_type,
    const GURL& url,
    metrics::TranslateEventProto* translate_event) {
  DCHECK(metrics::TranslateEventProto::EventType_IsValid(event_type));
  translate_event->set_event_type(
      static_cast<metrics::TranslateEventProto::EventType>(event_type));
  translate_event->set_event_timestamp_sec(
      (base::TimeTicks::Now() - base::TimeTicks()).InSeconds());
  AddTranslateEvent(*translate_event, url);
}

bool TranslateRankerImpl::ShouldOverrideDecision(
    int event_type,
    const GURL& url,
    metrics::TranslateEventProto* translate_event) {
  DCHECK(metrics::TranslateEventProto::EventType_IsValid(event_type));
  if (is_decision_override_enabled_) {
    translate_event->add_decision_overrides(
        static_cast<metrics::TranslateEventProto::EventType>(event_type));
    DVLOG(3) << "Overriding decision of type: " << event_type;
    return true;
  } else {
    RecordTranslateEvent(event_type, url, translate_event);
    return false;
  }
}

}  // namespace translate

std::ostream& operator<<(std::ostream& stream,
                         const translate::TranslateRankerFeatures& features) {
  features.WriteTo(stream);
  return stream;
}
