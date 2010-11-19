// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class is used by the RenderView to interact with a PhishingClassifier.

#ifndef CHROME_RENDERER_SAFE_BROWSING_PHISHING_CLASSIFIER_DELEGATE_H_
#define CHROME_RENDERER_SAFE_BROWSING_PHISHING_CLASSIFIER_DELEGATE_H_
#pragma once

#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"

class RenderView;

namespace WebKit {
class WebFrame;
}

namespace safe_browsing {
class PhishingClassifier;
class Scorer;

class PhishingClassifierDelegate {
 public:
  // The RenderView owns us.  This object takes ownership of the classifier.
  // Note that if classifier is null, a default instance of PhishingClassifier
  // will be used.
  PhishingClassifierDelegate(RenderView* render_view,
                             PhishingClassifier* classifier);
  ~PhishingClassifierDelegate();

  // Called by the RenderView once there is a phishing scorer available.
  // The scorer is passed on to the classifier.
  void SetPhishingScorer(const safe_browsing::Scorer* scorer);

  // Called by the RenderView when a page has started loading in the given
  // WebFrame.  Typically, this will cause any pending classification to be
  // cancelled.  However, if the load is for the main frame, and the toplevel
  // URL has not changed, we continue running the current classification.
  void CommittedLoadInFrame(WebKit::WebFrame* frame);

  // Called by the RenderView once a page has finished loading.  Determines
  // whether a new toplevel load has taken place, and if so, begins
  // classification.  May modify page_text.  Note that it is an error to
  // call OnNavigate if there is a pending classification.
  void FinishedLoad(string16* page_text);

  // Cancels any pending classification and frees the page text.  Called by
  // the RenderView when the RenderView is going away.
  void CancelPendingClassification();

 private:
  friend class PhishingClassifierDelegateTest;

  // Called when classification for the current page finishes.
  void ClassificationDone(bool is_phishy, double phishy_score);

  // Returns the RenderView's toplevel URL, with the ref stripped.
  GURL StripToplevelUrl();

  // The RenderView that owns this object.
  RenderView* render_view_;

  // The PhishingClassifier to use for the RenderView.  This is created once
  // a scorer is made available via SetPhishingScorer().
  scoped_ptr<PhishingClassifier> classifier_;

  // The last URL that was sent to the phishing classifier.
  GURL last_url_sent_to_classifier_;

  // The page id of the last load that was sent to the phishing classifier.
  // This is used to suppress phishing classification on back and forward
  // navigations in history.
  int32 last_page_id_sent_to_classifier_;

  // The page text that will be analyzed by the phishing classifier.  This is
  // set by OnNavigate and cleared when the classifier finishes.  Note that if
  // there is no classifier yet when OnNavigate is called, the page text will
  // be cached until the scorer is set and a classifier can be created.
  string16 classifier_page_text_;

  // Set to true if we should run the phishing classifier on the current page
  // as soon as SetPhishingScorer() is called.
  bool pending_classification_;

  DISALLOW_COPY_AND_ASSIGN(PhishingClassifierDelegate);
};

}  // namespace safe_browsing

#endif  // CHROME_RENDERER_SAFE_BROWSING_PHISHING_CLASSIFIER_DELEGATE_H_
