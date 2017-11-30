// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrome/browser/android/contextualsearch/contextual_search_ranker_logger_impl.h>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/assist_ranker/assist_ranker_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "components/assist_ranker/assist_ranker_service_impl.h"
#include "components/assist_ranker/binary_classifier_predictor.h"
#include "components/assist_ranker/proto/ranker_example.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents.h"
#include "jni/ContextualSearchRankerLoggerImpl_jni.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

namespace {

const char kContextualSearchRankerModelUrlParamName[] =
    "contextual-search-ranker-model-url";
const char kContextualSearchModelFilename[] = "contextual_search_model";
const char kContextualSearchUmaPrefix[] = "Search.ContextualSearch.Ranker";

const char kContextualSearchRankerDidPredict[] = "OutcomeRankerDidPredict";
const char kContextualSearchRankerPrediction[] = "OutcomeRankerPrediction";

const base::FeatureParam<std::string> kModelUrl{
    &chrome::android::kContextualSearchRankerQuery,
    kContextualSearchRankerModelUrlParamName, ""};

// TODO(donnd, hamelphi): move hex-hash-string to Ranker.
std::string HexHashFeatureName(const std::string& feature_name) {
  uint64_t feature_key = base::HashMetricName(feature_name);
  return base::StringPrintf("%016" PRIx64, feature_key);
}

}  // namespace

ContextualSearchRankerLoggerImpl::ContextualSearchRankerLoggerImpl(JNIEnv* env,
                                                                   jobject obj)
    : ukm_recorder_(nullptr),
      source_id_(0),
      builder_(nullptr),
      predictor_(nullptr),
      browser_context_(nullptr),
      ranker_example_(nullptr),
      has_predicted_decision_(false),
      java_object_(nullptr) {
  java_object_.Reset(env, obj);
}

ContextualSearchRankerLoggerImpl::~ContextualSearchRankerLoggerImpl() {
  java_object_ = nullptr;
}

void ContextualSearchRankerLoggerImpl::SetupLoggingAndRanker(
    JNIEnv* env,
    jobject obj,
    const base::android::JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  if (!web_contents)
    return;

  GURL page_url = web_contents->GetURL();
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  SetUkmRecorder(ukm_recorder, page_url);

  if (IsRankerQueryEnabled()) {
    SetupRankerPredictor(web_contents);
    // Start building example data based on features to be gathered and logged.
    ranker_example_.reset(new assist_ranker::RankerExample());
  } else {
    // TODO(donnd): remove when behind-the-flag bug fixed (crbug.com/786589).
    VLOG(1) << "SetupLoggingAndRanker got IsRankerQueryEnabled false.";
  }
}

void ContextualSearchRankerLoggerImpl::SetUkmRecorder(
    ukm::UkmRecorder* ukm_recorder,
    const GURL& page_url) {
  if (!ukm_recorder) {
    builder_.reset();
    return;
  }

  ukm_recorder_ = ukm_recorder;
  source_id_ = ukm_recorder_->GetNewSourceID();
  ukm_recorder_->UpdateSourceURL(source_id_, page_url);
  builder_ = ukm_recorder_->GetEntryBuilder(source_id_, "ContextualSearch");
}

void ContextualSearchRankerLoggerImpl::SetupRankerPredictor(
    content::WebContents* web_contents) {
  // Set up the Ranker predictor.
  if (IsRankerQueryEnabled()) {
    // Create one predictor for the current BrowserContext.
    content::BrowserContext* browser_context =
        web_contents->GetBrowserContext();
    if (browser_context == browser_context_)
      return;

    browser_context_ = browser_context;
    assist_ranker::AssistRankerService* assist_ranker_service =
        assist_ranker::AssistRankerServiceFactory::GetForBrowserContext(
            browser_context);
    DCHECK(assist_ranker_service);
    std::string model_string(kModelUrl.Get());
    DCHECK(model_string.size());
    // TODO(donnd): remove when behind-the-flag bug fixed (crbug.com/786589).
    VLOG(0) << "Model URL: " << model_string;
    predictor_ = assist_ranker_service->FetchBinaryClassifierPredictor(
        GURL(model_string), kContextualSearchModelFilename,
        kContextualSearchUmaPrefix);
    DCHECK(predictor_);
  }
}

void ContextualSearchRankerLoggerImpl::LogLong(
    JNIEnv* env,
    jobject obj,
    const base::android::JavaParamRef<jstring>& j_feature,
    jlong j_long) {
  std::string feature = base::android::ConvertJavaStringToUTF8(env, j_feature);
  if (builder_)
    builder_->AddMetric(feature.c_str(), j_long);

  // Also write to Ranker if we're logging data needed to predict a decision.
  if (IsRankerQueryEnabled() && !has_predicted_decision_) {
    std::string hex_feature_key(HexHashFeatureName(feature));
    auto& features = *ranker_example_->mutable_features();
    features[hex_feature_key].set_int32_value(j_long);
  }
}

AssistRankerPrediction ContextualSearchRankerLoggerImpl::RunInference(
    JNIEnv* env,
    jobject obj) {
  has_predicted_decision_ = true;
  bool prediction = false;
  bool was_able_to_predict = false;
  if (IsRankerQueryEnabled()) {
    was_able_to_predict = predictor_->Predict(*ranker_example_, &prediction);
    // Log to UMA whether we were able to predict or not.
    base::UmaHistogramBoolean("Search.ContextualSearchRankerWasAbleToPredict",
                              was_able_to_predict);
    // Log the Ranker decision to UKM, including whether we were able to make
    // any prediction.
    if (builder_) {
      builder_->AddMetric(kContextualSearchRankerDidPredict,
                          was_able_to_predict);
      if (was_able_to_predict) {
        builder_->AddMetric(kContextualSearchRankerPrediction, prediction);
      }
    }
  }
  AssistRankerPrediction prediction_enum;
  if (was_able_to_predict) {
    if (prediction) {
      prediction_enum = ASSIST_RANKER_PREDICTION_SHOW;
    } else {
      prediction_enum = ASSIST_RANKER_PREDICTION_SUPPRESS;
    }
  } else {
    prediction_enum = ASSIST_RANKER_PREDICTION_UNAVAILABLE;
  }
  // TODO(donnd): remove when behind-the-flag bug fixed (crbug.com/786589).
  VLOG(0) << "prediction: " << prediction_enum;
  return prediction_enum;
}

void ContextualSearchRankerLoggerImpl::WriteLogAndReset(JNIEnv* env,
                                                        jobject obj) {
  has_predicted_decision_ = false;
  if (!ukm_recorder_)
    return;

  // Set up another builder for the next record (in case it's needed).
  builder_ = ukm_recorder_->GetEntryBuilder(source_id_, "ContextualSearch");
  ranker_example_.reset();
}

bool ContextualSearchRankerLoggerImpl::IsRankerQueryEnabled() {
  return base::FeatureList::IsEnabled(
      chrome::android::kContextualSearchRankerQuery);
}

// Java wrapper boilerplate

void ContextualSearchRankerLoggerImpl::Destroy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  delete this;
}

jlong JNI_ContextualSearchRankerLoggerImpl_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  ContextualSearchRankerLoggerImpl* ranker_logger_impl =
      new ContextualSearchRankerLoggerImpl(env, obj);
  return reinterpret_cast<intptr_t>(ranker_logger_impl);
}
