// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/phishing_classifier.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/safe_browsing/feature_extractor_clock.h"
#include "chrome/renderer/safe_browsing/features.h"
#include "chrome/renderer/safe_browsing/phishing_dom_feature_extractor.h"
#include "chrome/renderer/safe_browsing/phishing_term_feature_extractor.h"
#include "chrome/renderer/safe_browsing/phishing_url_feature_extractor.h"
#include "chrome/renderer/safe_browsing/scorer.h"
#include "content/public/renderer/render_view.h"
#include "crypto/sha2.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

namespace safe_browsing {

const float PhishingClassifier::kInvalidScore = -1.0;
const float PhishingClassifier::kPhishyThreshold = 0.5;

PhishingClassifier::PhishingClassifier(content::RenderView* render_view,
                                       FeatureExtractorClock* clock)
    : render_view_(render_view),
      scorer_(NULL),
      clock_(clock),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  Clear();
}

PhishingClassifier::~PhishingClassifier() {
  // The RenderView should have called CancelPendingClassification() before
  // we are destroyed.
  CheckNoPendingClassification();
}

void PhishingClassifier::set_phishing_scorer(const Scorer* scorer) {
  CheckNoPendingClassification();
  scorer_ = scorer;
  if (scorer_) {
    url_extractor_.reset(new PhishingUrlFeatureExtractor);
    dom_extractor_.reset(
        new PhishingDOMFeatureExtractor(render_view_, clock_.get()));
    term_extractor_.reset(new PhishingTermFeatureExtractor(
        &scorer_->page_terms(),
        &scorer_->page_words(),
        scorer_->max_words_per_term(),
        scorer_->murmurhash3_seed(),
        clock_.get()));
  } else {
    // We're disabling client-side phishing detection, so tear down all
    // of the relevant objects.
    url_extractor_.reset();
    dom_extractor_.reset();
    term_extractor_.reset();
  }
}

bool PhishingClassifier::is_ready() const {
  return scorer_ != NULL;
}

void PhishingClassifier::BeginClassification(
    const string16* page_text,
    const DoneCallback& done_callback) {
  DCHECK(is_ready());

  // The RenderView should have called CancelPendingClassification() before
  // starting a new classification, so DCHECK this.
  CheckNoPendingClassification();
  // However, in an opt build, we will go ahead and clean up the pending
  // classification so that we can start in a known state.
  CancelPendingClassification();

  page_text_ = page_text;
  done_callback_ = done_callback;

  // For consistency, we always want to invoke the DoneCallback
  // asynchronously, rather than directly from this method.  To ensure that
  // this is the case, post a task to begin feature extraction on the next
  // iteration of the message loop.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PhishingClassifier::BeginFeatureExtraction,
                 weak_factory_.GetWeakPtr()));
}

void PhishingClassifier::BeginFeatureExtraction() {
  WebKit::WebView* web_view = render_view_->GetWebView();
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
  GURL url(frame->document().url());
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
      base::Bind(&PhishingClassifier::DOMExtractionFinished,
                 base::Unretained(this)));
}

void PhishingClassifier::CancelPendingClassification() {
  // Note that cancelling the feature extractors is simply a no-op if they
  // were not running.
  DCHECK(is_ready());
  dom_extractor_->CancelPendingExtraction();
  term_extractor_->CancelPendingExtraction();
  weak_factory_.InvalidateWeakPtrs();
  Clear();
}

void PhishingClassifier::DOMExtractionFinished(bool success) {
  if (success) {
    // Term feature extraction can take awhile, so it runs asynchronously
    // in several chunks of work and invokes the callback when finished.
    term_extractor_->ExtractFeatures(
        page_text_,
        features_.get(),
        base::Bind(&PhishingClassifier::TermExtractionFinished,
                   base::Unretained(this)));
  } else {
    RunFailureCallback();
  }
}

void PhishingClassifier::TermExtractionFinished(bool success) {
  if (success) {
    WebKit::WebView* web_view = render_view_->GetWebView();
    if (!web_view) {
      RunFailureCallback();
      return;
    }
    WebKit::WebFrame* main_frame = web_view->mainFrame();
    if (!main_frame) {
      RunFailureCallback();
      return;
    }

    // Hash all of the features so that they match the model, then compute
    // the score.
    FeatureMap hashed_features;
    ClientPhishingRequest verdict;
    verdict.set_model_version(scorer_->model_version());
    verdict.set_url(main_frame->document().url().spec());
    for (base::hash_map<std::string, double>::const_iterator it =
             features_->features().begin();
         it != features_->features().end(); ++it) {
      VLOG(2) << "Feature: " << it->first << " = " << it->second;
      bool result = hashed_features.AddRealFeature(
          crypto::SHA256HashString(it->first), it->second);
      DCHECK(result);
      ClientPhishingRequest::Feature* feature = verdict.add_feature_map();
      feature->set_name(it->first);
      feature->set_value(it->second);
    }
    float score = static_cast<float>(scorer_->ComputeScore(hashed_features));
    verdict.set_client_score(score);
    verdict.set_is_phishing(score >= kPhishyThreshold);
    RunCallback(verdict);
  } else {
    RunFailureCallback();
  }
}

void PhishingClassifier::CheckNoPendingClassification() {
  DCHECK(done_callback_.is_null());
  DCHECK(!page_text_);
  if (done_callback_.is_null() || page_text_) {
    LOG(ERROR) << "Classification in progress, missing call to "
               << "CancelPendingClassification";
    UMA_HISTOGRAM_COUNTS("SBClientPhishing.CheckNoPendingClassificationFailed",
                         1);
  }
}

void PhishingClassifier::RunCallback(const ClientPhishingRequest& verdict) {
  done_callback_.Run(verdict);
  Clear();
}

void PhishingClassifier::RunFailureCallback() {
  ClientPhishingRequest verdict;
  // In this case we're not guaranteed to have a valid URL.  Just set it
  // to the empty string to make sure we have a valid protocol buffer.
  verdict.set_url("");
  verdict.set_client_score(kInvalidScore);
  verdict.set_is_phishing(false);
  RunCallback(verdict);
}

void PhishingClassifier::Clear() {
  page_text_ = NULL;
  done_callback_.Reset();
  features_.reset(NULL);
}

}  // namespace safe_browsing
