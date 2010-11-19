// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/phishing_classifier_delegate.h"

#include "base/callback.h"
#include "base/logging.h"
#include "chrome/renderer/navigation_state.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/safe_browsing/feature_extractor_clock.h"
#include "chrome/renderer/safe_browsing/phishing_classifier.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"

namespace safe_browsing {

PhishingClassifierDelegate::PhishingClassifierDelegate(
    RenderView* render_view,
    PhishingClassifier* classifier)
    : render_view_(render_view),
      last_page_id_sent_to_classifier_(-1),
      pending_classification_(false) {
  if (!classifier) {
    classifier = new PhishingClassifier(render_view_,
                                        new FeatureExtractorClock());
  }
  classifier_.reset(classifier);
}

PhishingClassifierDelegate::~PhishingClassifierDelegate() {
  CancelPendingClassification();
}

void PhishingClassifierDelegate::SetPhishingScorer(
    const safe_browsing::Scorer* scorer) {
  classifier_->set_phishing_scorer(scorer);

  if (pending_classification_) {
    pending_classification_ = false;
    // If we have a pending classificaton, it should always be true that the
    // main frame URL and page id have not changed since we queued the
    // classification.  This is because we stop any pending classification on
    // main frame loads in RenderView::didCommitProvisionalLoad().
    DCHECK_EQ(StripToplevelUrl(), last_url_sent_to_classifier_);
    DCHECK_EQ(render_view_->page_id(), last_page_id_sent_to_classifier_);
    classifier_->BeginClassification(
        &classifier_page_text_,
        NewCallback(this, &PhishingClassifierDelegate::ClassificationDone));
  }
}

void PhishingClassifierDelegate::CommittedLoadInFrame(
    WebKit::WebFrame* frame) {
  // A new page is starting to load.  Unless the load is a navigation within
  // the same page, we need to cancel classification since the content will
  // now be inconsistent with the phishing model.
  NavigationState* state = NavigationState::FromDataSource(
      frame->dataSource());
  if (!state->was_within_same_page()) {
    CancelPendingClassification();
  }
}

void PhishingClassifierDelegate::FinishedLoad(string16* page_text) {
  // We check that the page id has incremented so that we don't reclassify
  // pages as the user moves back and forward in session history.  Note: we
  // don't send every page id to the classifier, only those where the toplevel
  // URL changed.
  int load_id = render_view_->page_id();
  if (load_id <= last_page_id_sent_to_classifier_) {
    return;
  }

  GURL url_without_ref = StripToplevelUrl();
  if (url_without_ref == last_url_sent_to_classifier_) {
    // The toplevle URL is the same, except for the ref.
    // Update the last page id we sent, but don't trigger a new classification.
    last_page_id_sent_to_classifier_ = load_id;
    return;
  }

  last_url_sent_to_classifier_ = url_without_ref;
  last_page_id_sent_to_classifier_ = load_id;
  classifier_page_text_.swap(*page_text);

  if (classifier_->is_ready()) {
    classifier_->BeginClassification(
        &classifier_page_text_,
        NewCallback(this, &PhishingClassifierDelegate::ClassificationDone));
  } else {
    // If there is no phishing classifier yet, we'll begin classification once
    // SetPhishingScorer() is called by the RenderView.
    pending_classification_ = true;
  }
}

void PhishingClassifierDelegate::CancelPendingClassification() {
  if (classifier_->is_ready()) {
    classifier_->CancelPendingClassification();
  }
  classifier_page_text_.clear();
  pending_classification_ = false;
}

void PhishingClassifierDelegate::ClassificationDone(bool is_phishy,
                                                    double phishy_score) {
  // We no longer need the page text.
  classifier_page_text_.clear();
  VLOG(2) << "Phishy verdict = " << is_phishy << " score = " << phishy_score;

  // TODO(bryner): Grab a snapshot and send a DetectedPhishingSite message
  // to the browser.
}

GURL PhishingClassifierDelegate::StripToplevelUrl() {
  GURL toplevel_url = render_view_->webview()->mainFrame()->url();
  GURL::Replacements replacements;
  replacements.ClearRef();
  return toplevel_url.ReplaceComponents(replacements);
}

}  // namespace safe_browsing
