// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class handles the process of extracting all of the features from a
// page and computing a phishyness score.  The basic steps are:
//  - Run each feature extractor over the page, building up a FeatureMap of
//    feature -> value.
//  - SHA-256 hash all of the feature names in the map so that they match the
//    supplied model.
//  - Hand the hashed map off to a Scorer, which computes the probability that
//    the page is phishy.
//  - If the page is phishy, run the supplied callback.
//
// For more details, see phishing_*_feature_extractor.h, scorer.h, and
// client_model.proto.

#ifndef CHROME_RENDERER_SAFE_BROWSING_PHISHING_CLASSIFIER_H_
#define CHROME_RENDERER_SAFE_BROWSING_PHISHING_CLASSIFIER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/task.h"

class RenderView;

namespace safe_browsing {
class FeatureExtractorClock;
class FeatureMap;
class PhishingDOMFeatureExtractor;
class PhishingTermFeatureExtractor;
class PhishingUrlFeatureExtractor;
class Scorer;

class PhishingClassifier {
 public:
  // Callback to be run when phishing classification finishes.  If the first
  // argument is true, the page is considered phishy by the client-side model,
  // and the browser should ping back to get a final verdict.  The second
  // argument gives the phishyness score which is used in the pingback,
  // or kInvalidScore if classification failed.
  typedef Callback2<bool /* phishy */, double /* phishy_score */>::Type
    DoneCallback;

  static const double kInvalidScore;

  // Creates a new PhishingClassifier object that will operate on
  // |render_view|.  |clock| is used to time feature extractor operations, and
  // the PhishingClassifier takes ownership of this object.  Note that the
  // classifier will not be 'ready' until set_phishing_scorer() is called.
  PhishingClassifier(RenderView* render_view, FeatureExtractorClock* clock);
  virtual ~PhishingClassifier();

  // Sets a scorer for the classifier to use in computing the phishiness score.
  // This must live at least as long as the PhishingClassifier.
  void set_phishing_scorer(const Scorer* scorer);

  // Returns true if the classifier is ready to classify pages, i.e. it
  // has had a scorer set via set_phishing_scorer().
  bool is_ready() const;

  // Called by the RenderView when a page has finished loading.  This begins
  // the feature extraction and scoring process. |page_text| should contain
  // the plain text of a web page, including any subframes, as returned by
  // RenderView::CaptureText().  |page_text| is owned by the caller, and must
  // not be destroyed until either |done_callback| is run or
  // CancelPendingClassification() is called.
  //
  // To avoid blocking the render thread for too long, phishing classification
  // may run in several chunks of work, posting a task to the current
  // MessageLoop to continue processing.  Once the scoring process is complete,
  // |done_callback| is run on the current thread.  PhishingClassifier takes
  // ownership of the callback.
  //
  // It is an error to call BeginClassification if the classifier is not yet
  // ready.
  virtual void BeginClassification(const string16* page_text,
                                   DoneCallback* callback);

  // Called by the RenderView (on the render thread) when a page is unloading
  // or the RenderView is being destroyed.  This cancels any extraction that
  // is in progress.  It is an error to call CancelPendingClassification if
  // the classifier is not yet ready.
  virtual void CancelPendingClassification();

 private:
  // Any score equal to or above this value is considered phishy.
  static const double kPhishyThreshold;

  // Begins the feature extraction process, by extracting URL features and
  // beginning DOM feature extraction.
  void BeginFeatureExtraction();

  // Callback to be run when DOM feature extraction is complete.
  // If it was successful, begins term feature extraction, otherwise
  // runs the DoneCallback with a non-phishy verdict.
  void DOMExtractionFinished(bool success);

  // Callback to be run when term feature extraction is complete.
  // If it was successful, computes a score and runs the DoneCallback.
  // If extraction was unsuccessful, runs the DoneCallback with a
  // non-phishy verdict.
  void TermExtractionFinished(bool success);

  // Helper to verify that there is no pending phishing classification.  Dies
  // in debug builds if the state is not as expected.  This is a no-op in
  // release builds.
  void CheckNoPendingClassification();

  // Helper method to run the DoneCallback and clear the state.
  void RunCallback(bool phishy, double phishy_score);

  // Helper to run the DoneCallback when feature extraction has failed.
  // This always signals a non-phishy verdict for the page, with kInvalidScore.
  void RunFailureCallback();

  // Clears the current state of the PhishingClassifier.
  void Clear();

  RenderView* render_view_;  // owns us
  const Scorer* scorer_;  // owned by the caller
  scoped_ptr<FeatureExtractorClock> clock_;
  scoped_ptr<PhishingUrlFeatureExtractor> url_extractor_;
  scoped_ptr<PhishingDOMFeatureExtractor> dom_extractor_;
  scoped_ptr<PhishingTermFeatureExtractor> term_extractor_;

  // State for any in-progress extraction.
  scoped_ptr<FeatureMap> features_;
  const string16* page_text_;  // owned by the caller
  scoped_ptr<DoneCallback> done_callback_;

  // Used to create BeginFeatureExtraction tasks.
  // These tasks are revoked if classification is cancelled.
  ScopedRunnableMethodFactory<PhishingClassifier> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(PhishingClassifier);
};

}  // namespace safe_browsing

#endif  // CHROME_RENDERER_SAFE_BROWSING_PHISHING_CLASSIFIER_H_
