// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_RANKER_LOGGER_IMPL_H_
#define CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_RANKER_LOGGER_IMPL_H_

#include "base/android/jni_android.h"

class GURL;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace assist_ranker {
class BinaryClassifierPredictor;
class RankerExample;
}  // namespace assist_ranker

namespace ukm {
class UkmEntryBuilder;
class UkmRecorder;
}  // namespace ukm

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.contextualsearch
enum AssistRankerPrediction {
  ASSIST_RANKER_PREDICTION_UNDETERMINED,
  ASSIST_RANKER_PREDICTION_UNAVAILABLE,
  ASSIST_RANKER_PREDICTION_SUPPRESS,
  ASSIST_RANKER_PREDICTION_SHOW,
};

// Runs Ranker inference and logging through UKM for Ranker model development.
// This is used to prediction whether a tap gesture will be useful to the user
// or not.
class ContextualSearchRankerLoggerImpl {
 public:
  ContextualSearchRankerLoggerImpl(JNIEnv* env, jobject obj);
  ~ContextualSearchRankerLoggerImpl();

  // Calls the destructor.  Should be called when this native object is no
  // longer needed.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Sets up the logging and Ranker for Contextual Search features using the
  // given details.
  // |java_web_contents| is the |WebContents| of the base-page (where the user
  // tapped).
  void SetupLoggingAndRanker(
      JNIEnv* env,
      jobject obj,
      const base::android::JavaParamRef<jobject>& java_web_contents);

  // Logs a long value with the given feature name.
  void LogLong(JNIEnv* env,
               jobject obj,
               const base::android::JavaParamRef<jstring>& j_feature,
               jlong j_long);

  // Runs the model and returns the inference result as an
  // AssistRankerPrediction enum.
  AssistRankerPrediction RunInference(JNIEnv* env, jobject obj);

  // Writes the currently logged data and resets the current builder to be
  // ready to start logging the next set of data.
  void WriteLogAndReset(JNIEnv* env, jobject obj);

 private:
  // Set the UKM recorder and base-page URL.
  // TODO(donnd): write a test, using this to inject a test-ukm-recorder.
  void SetUkmRecorder(ukm::UkmRecorder* ukm_recorder, const GURL& page_url);

  // Sets up the Ranker Predictor for the given |web_contents|.
  void SetupRankerPredictor(content::WebContents* web_contents);

  // Whether querying Ranker for model loading and prediction is enabled.
  bool IsRankerQueryEnabled();

  // Used to log URL-keyed metrics. This pointer will outlive |this|, and may
  // be nullptr.
  ukm::UkmRecorder* ukm_recorder_;

  // The UKM source ID being used for this session.
  int32_t source_id_;

  // The entry builder for the current record, or nullptr if not yet configured.
  std::unique_ptr<ukm::UkmEntryBuilder> builder_;

  // The Ranker Predictor for whether a tap gesture should be suppressed or not.
  std::unique_ptr<assist_ranker::BinaryClassifierPredictor> predictor_;

  // The |BrowserContext| currently associated with the above predictor.
  content::BrowserContext* browser_context_;

  // The current RankerExample or null.
  // Set of features from one example of a Tap to predict a suppression
  // decision.
  std::unique_ptr<assist_ranker::RankerExample> ranker_example_;

  // Whether Ranker has predicted the decision yet.
  bool has_predicted_decision_;

  // The linked Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(ContextualSearchRankerLoggerImpl);
};

#endif  // CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_RANKER_LOGGER_IMPL_H_
