// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/phishing_classifier.h"

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/sha2.h"
#include "base/string_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/safe_browsing/feature_extractor_clock.h"
#include "chrome/renderer/safe_browsing/features.h"
#include "chrome/renderer/safe_browsing/phishing_dom_feature_extractor.h"
#include "chrome/renderer/safe_browsing/phishing_term_feature_extractor.h"
#include "chrome/renderer/safe_browsing/phishing_url_feature_extractor.h"
#include "chrome/renderer/safe_browsing/scorer.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

namespace safe_browsing {

const double PhishingClassifier::kInvalidScore = -1.0;
const double PhishingClassifier::kPhishyThreshold = 0.5;

PhishingClassifier::PhishingClassifier(RenderView* render_view,
                                       FeatureExtractorClock* clock)
    : render_view_(render_view),
      scorer_(NULL),
      clock_(clock),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  Clear();
}

PhishingClassifier::~PhishingClassifier() {
  // The RenderView should have called CancelPendingClassification() before
  // we are destroyed.
  CheckNoPendingClassification();
}

void PhishingClassifier::set_phishing_scorer(const Scorer* scorer) {
  DCHECK(!scorer_);
  scorer_ = scorer;
  url_extractor_.reset(new PhishingUrlFeatureExtractor);
  dom_extractor_.reset(
      new PhishingDOMFeatureExtractor(render_view_, clock_.get()));
  term_extractor_.reset(new PhishingTermFeatureExtractor(
      &scorer_->page_terms(),
      &scorer_->page_words(),
      scorer_->max_words_per_term(),
      clock_.get()));
}

bool PhishingClassifier::is_ready() const {
  return scorer_ != NULL;
}

void PhishingClassifier::BeginClassification(const string16* page_text,
                                             DoneCallback* done_callback) {
  DCHECK(is_ready());

  // The RenderView should have called CancelPendingClassification() before
  // starting a new classification, so DCHECK this.
  CheckNoPendingClassification();
  // However, in an opt build, we will go ahead and clean up the pending
  // classification so that we can start in a known state.
  CancelPendingClassification();

  page_text_ = page_text;
  done_callback_.reset(done_callback);

  // For consistency, we always want to invoke the DoneCallback
  // asynchronously, rather than directly from this method.  To ensure that
  // this is the case, post a task to begin feature extraction on the next
  // iteration of the message loop.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &PhishingClassifier::BeginFeatureExtraction));
}

void PhishingClassifier::BeginFeatureExtraction() {
  WebKit::WebView* web_view = render_view_->webview();
  if (!web_view) {
    RunFailureCallback();
    return;
  }

  WebKit::WebFrame* frame = web_view->mainFrame();
  if (!frame) {
    RunFailureCallback();
    return;
  }

  // Check whether the URL is one that we should classify.
  // Currently, we only classify http: URLs that are GET requests.
  GURL url(frame->url());
  if (!url.SchemeIs(chrome::kHttpScheme)) {
    RunFailureCallback();
    return;
  }

  WebKit::WebDataSource* ds = frame->dataSource();
  if (!ds || !EqualsASCII(ds->request().httpMethod(), "GET")) {
    RunFailureCallback();
    return;
  }

  features_.reset(new FeatureMap);
  if (!url_extractor_->ExtractFeatures(url, features_.get())) {
    RunFailureCallback();
    return;
  }

  // DOM feature extraction can take awhile, so it runs asynchronously
  // in several chunks of work and invokes the callback when finished.
  dom_extractor_->ExtractFeatures(
      features_.get(),
      NewCallback(this, &PhishingClassifier::DOMExtractionFinished));
}

void PhishingClassifier::CancelPendingClassification() {
  // Note that cancelling the feature extractors is simply a no-op if they
  // were not running.
  DCHECK(is_ready());
  dom_extractor_->CancelPendingExtraction();
  term_extractor_->CancelPendingExtraction();
  method_factory_.RevokeAll();
  Clear();
}

void PhishingClassifier::DOMExtractionFinished(bool success) {
  if (success) {
    // Term feature extraction can take awhile, so it runs asynchronously
    // in several chunks of work and invokes the callback when finished.
    term_extractor_->ExtractFeatures(
        page_text_,
        features_.get(),
        NewCallback(this, &PhishingClassifier::TermExtractionFinished));
  } else {
    RunFailureCallback();
  }
}

void PhishingClassifier::TermExtractionFinished(bool success) {
  if (success) {
    // Hash all of the features so that they match the model, then compute
    // the score.
    FeatureMap hashed_features;
    for (base::hash_map<std::string, double>::const_iterator it =
             features_->features().begin();
         it != features_->features().end(); ++it) {
      bool result = hashed_features.AddRealFeature(
          base::SHA256HashString(it->first), it->second);
      DCHECK(result);
    }

    double score = scorer_->ComputeScore(hashed_features);
    RunCallback(score >= kPhishyThreshold, score);
  } else {
    RunFailureCallback();
  }
}

void PhishingClassifier::CheckNoPendingClassification() {
  DCHECK(!done_callback_.get());
  DCHECK(!page_text_);
  if (done_callback_.get() || page_text_) {
    LOG(ERROR) << "Classification in progress, missing call to "
               << "CancelPendingClassification";
  }
}

void PhishingClassifier::RunCallback(bool phishy, double phishy_score) {
  done_callback_->Run(phishy, phishy_score);
  Clear();
}

void PhishingClassifier::RunFailureCallback() {
  RunCallback(false /* not phishy */, kInvalidScore);
}

void PhishingClassifier::Clear() {
  page_text_ = NULL;
  done_callback_.reset(NULL);
  features_.reset(NULL);
}

}  // namespace safe_browsing
