// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_ranker.h"

#include <cmath>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/string_util.h"
#include "components/metrics/proto/translate_event.pb.h"
#include "components/translate/core/browser/proto/translate_ranker_model.pb.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_url_fetcher.h"
#include "components/translate/core/common/translate_switches.h"

namespace translate {

namespace {

typedef google::protobuf::Map<std::string, float> WeightMap;

const double kTranslationOfferThreshold = 0.5;

// Parameters for model fetching.
const char kTranslateRankerModelURL[] =
    "https://chromium-i18n.appspot.com/ssl-translate-ranker-model";
const int kMaxRetryOn5xx = 3;
const int kDownloadRefractoryPeriodMin = 15;
const char kUnknown[] = "UNKNOWN";

// Enumeration denoting the outcome of an attempt to download the translate
// ranker model. This must be kept in sync with the TranslateRankerModelStatus
// enum in histograms.xml
enum ModelStatus {
  MODEL_STATUS_OK = 0,
  MODEL_STATUS_DOWNLOAD_THROTTLED = 1,
  MODEL_STATUS_DOWNLOAD_FAILED = 2,
  MODEL_STATUS_PARSE_FAILED = 3,
  MODEL_STATUS_VALIDATION_FAILED = 4,
  // Insert new values above this line.
  MODEL_STATUS_MAX
};

double Sigmoid(double x) {
  return 1.0 / (1.0 + exp(-x));
}

double ScoreComponent(const WeightMap& weights, const std::string& key) {
  WeightMap::const_iterator i = weights.find(base::ToLowerASCII(key));
  if (i == weights.end())
    i = weights.find(kUnknown);
  return i == weights.end() ? 0.0 : i->second;
}

GURL GetTranslateRankerURL() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return GURL(command_line->HasSwitch(switches::kTranslateRankerModelURL)
                  ? command_line->GetSwitchValueASCII(
                        switches::kTranslateRankerModelURL)
                  : kTranslateRankerModelURL);
}

void ReportModelStatus(ModelStatus model_status) {
  UMA_HISTOGRAM_ENUMERATION("Translate.Ranker.Model.Status", model_status,
                            MODEL_STATUS_MAX);
}

}  // namespace

const base::Feature kTranslateRankerQuery{"TranslateRankerQuery",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTranslateRankerEnforcement{
    "TranslateRankerEnforcement", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTranslateRankerLogging{"TranslateRankerLogging",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

TranslateRanker::~TranslateRanker() {}

// static
bool TranslateRanker::IsEnabled() {
  return IsQueryEnabled() || IsEnforcementEnabled();
}

// static
bool TranslateRanker::IsLoggingEnabled() {
  return base::FeatureList::IsEnabled(kTranslateRankerLogging);
}

// static
bool TranslateRanker::IsQueryEnabled() {
  return base::FeatureList::IsEnabled(kTranslateRankerQuery);
}

// static
bool TranslateRanker::IsEnforcementEnabled() {
  return base::FeatureList::IsEnabled(kTranslateRankerEnforcement);
}

// static
TranslateRanker* TranslateRanker::GetInstance() {
  return base::Singleton<TranslateRanker>::get();
}

std::unique_ptr<TranslateRanker> TranslateRanker::CreateForTesting(
    const std::string& model_data) {
  std::unique_ptr<TranslateRanker> ranker(new TranslateRanker());
  CHECK(ranker != nullptr);
  ranker->ParseModel(0, true, model_data);
  CHECK(ranker->model_ != nullptr);
  return ranker;
}

bool TranslateRanker::ShouldOfferTranslation(
    const TranslatePrefs& translate_prefs,
    const std::string& src_lang,
    const std::string& dst_lang) {
  // The ranker is a gate in the "show a translation prompt" flow. To retain
  // the pre-existing functionality, it defaults to returning true in the
  // absence of a model or if enforcement is disabled. As this is ranker is
  // subsumed into a more general assist ranker, this default will go away
  // (or become False).
  const bool kDefaultResponse = true;

  // If we don't have a model, request one and return the default.
  if (model_ == nullptr) {
    FetchModelData();
    return kDefaultResponse;
  }

  DCHECK(model_->has_logistic_regression_model());

  SCOPED_UMA_HISTOGRAM_TIMER("Translate.Ranker.Timer.ShouldOfferTranslation");

  // TODO(rogerm): Remove ScopedTracker below once crbug.com/646711 is closed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "646711 translate::TranslateRanker::ShouldOfferTranslation"));

  const std::string& app_locale =
      TranslateDownloadManager::GetInstance()->application_locale();
  const std::string& country = translate_prefs.GetCountry();
  double accept_count = translate_prefs.GetTranslationAcceptedCount(src_lang);
  double denied_count = translate_prefs.GetTranslationDeniedCount(src_lang);
  double ignored_count =
      model_->logistic_regression_model().has_ignore_ratio_weight()
          ? translate_prefs.GetTranslationIgnoredCount(src_lang)
          : 0.0;
  double total_count = accept_count + denied_count + ignored_count;
  double accept_ratio =
      (total_count == 0.0) ? 0.0 : (accept_count / total_count);
  double decline_ratio =
      (total_count == 0.0) ? 0.0 : (denied_count / total_count);
  double ignore_ratio =
      (total_count == 0.0) ? 0.0 : (ignored_count / total_count);
  DVLOG(3) << "TranslateRanker: features=["
           << "src_lang='" << src_lang << "', dst_lang='" << dst_lang
           << "', country='" << country << "', locale='" << app_locale
           << ", accept_count=" << accept_count
           << ", denied_count=" << denied_count
           << ", ignored_count=" << ignored_count
           << ", total_count=" << total_count
           << ", accept_ratio=" << accept_ratio
           << ", decline_ratio=" << decline_ratio
           << ", ignore_ratio=" << ignore_ratio << "]";

  double score = CalculateScore(accept_ratio, decline_ratio, ignore_ratio,
                                src_lang, dst_lang, app_locale, country);

  DVLOG(2) << "TranslateRanker Score: " << score;

  bool result = (score >= kTranslationOfferThreshold);

  UMA_HISTOGRAM_BOOLEAN("Translate.Ranker.QueryResult", result);

  return result;
}

TranslateRanker::TranslateRanker() {}

double TranslateRanker::CalculateScore(double accept_ratio,
                                       double decline_ratio,
                                       double ignore_ratio,
                                       const std::string& src_lang,
                                       const std::string& dst_lang,
                                       const std::string& locale,
                                       const std::string& country) {
  SCOPED_UMA_HISTOGRAM_TIMER("Translate.Ranker.Timer.CalculateScore");
  DCHECK(model_ != nullptr);
  DCHECK(model_->has_logistic_regression_model());
  const chrome_intelligence::TranslateRankerModel::LogisticRegressionModel&
      logit = model_->logistic_regression_model();
  double dot_product =
      (accept_ratio * logit.accept_ratio_weight()) +
      (decline_ratio * logit.decline_ratio_weight()) +
      (ignore_ratio * logit.ignore_ratio_weight()) +
      ScoreComponent(logit.source_language_weight(), src_lang) +
      ScoreComponent(logit.dest_language_weight(), dst_lang) +
      ScoreComponent(logit.country_weight(), country) +
      ScoreComponent(logit.locale_weight(), locale);
  return Sigmoid(dot_product + logit.bias());
}

int TranslateRanker::GetModelVersion() const {
  return (model_ == nullptr) ? 0 : model_->version();
}

void TranslateRanker::FetchModelData() {
  // Exit if the model has already been successfully loaded.
  if (model_ != nullptr) {
    return;
  }

  // Exit if the download has been throttled.
  if (base::Time::NowFromSystemTime() < next_earliest_download_time_) {
    return;
  }

  // Create the model fetcher if it does not exist.
  if (model_fetcher_ == nullptr) {
    model_fetcher_.reset(new TranslateURLFetcher(kFetcherId));
    model_fetcher_->set_max_retry_on_5xx(kMaxRetryOn5xx);
  }

  // If a request is already in flight, do not issue a new one.
  if (model_fetcher_->state() == TranslateURLFetcher::REQUESTING) {
    DVLOG(2) << "TranslateRanker: Download complete or in progress.";
    return;
  }

  DVLOG(2) << "TranslateRanker: Downloading model...";

  download_start_time_ = base::Time::Now();
  bool result = model_fetcher_->Request(
      GetTranslateRankerURL(),
      base::Bind(&TranslateRanker::ParseModel, base::Unretained(this)));

  if (!result) {
    ReportModelStatus(MODEL_STATUS_DOWNLOAD_THROTTLED);
    next_earliest_download_time_ =
        base::Time::NowFromSystemTime() +
        base::TimeDelta::FromMinutes(kDownloadRefractoryPeriodMin);
  }
}

void TranslateRanker::ParseModel(int /* id */,
                                 bool success,
                                 const std::string& data) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Translate.Ranker.Timer.DownloadModel",
                             base::Time::Now() - download_start_time_);

  SCOPED_UMA_HISTOGRAM_TIMER("Translate.Ranker.Timer.ParseModel");

  // We should not be here if the model has already been downloaded and parsed.
  DCHECK(model_ == nullptr);

  // On failure, we just abort. The TranslateRanker will retry on a subsequent
  // translation opportunity. The TranslateURLFetcher enforces a limit for
  // retried requests.
  if (!success) {
    ReportModelStatus(MODEL_STATUS_DOWNLOAD_FAILED);
    return;
  }

  // Create a new model instance, parse and validate the data, and move it over
  // to be used by the ranker.
  std::unique_ptr<chrome_intelligence::TranslateRankerModel> new_model(
      new chrome_intelligence::TranslateRankerModel());

  bool is_parseable = new_model->ParseFromString(data);
  if (!is_parseable) {
    ReportModelStatus(MODEL_STATUS_PARSE_FAILED);
    return;
  }

  bool is_valid = new_model->has_logistic_regression_model();
  if (!is_valid) {
    ReportModelStatus(MODEL_STATUS_VALIDATION_FAILED);
    return;
  }

  ReportModelStatus(MODEL_STATUS_OK);
  model_ = std::move(new_model);
  model_fetcher_.reset();
}

void TranslateRanker::FlushTranslateEvents(
    std::vector<metrics::TranslateEventProto>* translate_events) {
  if (IsLoggingEnabled()) {
    translate_events->swap(translate_events_cache_);
    translate_events_cache_.clear();
  }
}

void TranslateRanker::RecordTranslateEvent(
    const metrics::TranslateEventProto& translate_event) {
  if (IsLoggingEnabled())
    translate_events_cache_.push_back(translate_event);
}

}  // namespace translate
