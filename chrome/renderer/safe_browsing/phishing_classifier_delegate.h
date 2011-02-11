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
#include "chrome/renderer/render_view_observer.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_platform_file.h"

namespace safe_browsing {
class PhishingClassifier;
class Scorer;

class PhishingClassifierDelegate : public RenderViewObserver {
 public:
  static void SetPhishingModel(IPC::PlatformFileForTransit model_file);

  // The RenderView owns us.  This object takes ownership of the classifier.
  // Note that if classifier is null, a default instance of PhishingClassifier
  // will be used.
  PhishingClassifierDelegate(RenderView* render_view,
                             PhishingClassifier* classifier);
  ~PhishingClassifierDelegate();

  // Called by the RenderView once there is a phishing scorer available.
  // The scorer is passed on to the classifier.
  void SetPhishingScorer(const safe_browsing::Scorer* scorer);

  // RenderViewObserver implementation, public for testing.

  // Called by the RenderView once a page has finished loading.  Updates the
  // last-loaded URL and page id, then starts classification if all other
  // conditions are met (see MaybeStartClassification for details).
  virtual void PageCaptured(const string16& page_text);

  // Called by the RenderView when a page has started loading in the given
  // WebFrame.  Typically, this will cause any pending classification to be
  // cancelled.  However, if the navigation is within the same page, we
  // continue running the current classification.
  virtual void DidCommitProvisionalLoad(WebKit::WebFrame* frame,
                                        bool is_new_navigation);

  // Cancels any pending classification and frees the page text.  Called by
  // the RenderView when the RenderView is going away.
  void CancelPendingClassification();

 private:
  friend class PhishingClassifierDelegateTest;

  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);

  // Called by the RenderView when it receives a StartPhishingDetection IPC
  // from the browser.  This signals that it is ok to begin classification
  // for the given toplevel URL.  If the URL has been fully loaded into the
  // RenderView and a Scorer has been set, this will begin classification,
  // otherwise classification will be deferred until these conditions are met.
  void OnStartPhishingDetection(const GURL& url);

  // Called when classification for the current page finishes.
  void ClassificationDone(bool is_phishy, double phishy_score);

  // Returns the RenderView's toplevel URL, with the ref stripped.
  GURL StripToplevelUrl();

  // Shared code to begin classification if all conditions are met.
  void MaybeStartClassification();

  // The PhishingClassifier to use for the RenderView.  This is created once
  // a scorer is made available via SetPhishingScorer().
  scoped_ptr<PhishingClassifier> classifier_;

  // The last URL that the browser instructed us to classify.
  GURL last_url_received_from_browser_;

  // The last URL and page id that have finished loading in the RenderView.
  // These correspond to the text in classifier_page_text_.
  GURL last_finished_load_url_;
  int32 last_finished_load_id_;

  // The URL and page id of the last load that we actually started
  // classification on.  This is used to suppress phishing classification on
  // subframe navigation and back and forward navigations in history.
  GURL last_url_sent_to_classifier_;
  int32 last_page_id_sent_to_classifier_;

  // The page text that will be analyzed by the phishing classifier.  This is
  // set by OnNavigate and cleared when the classifier finishes.  Note that if
  // there is no Scorer yet when OnNavigate is called, or the browser has not
  // instructed us to classify the page, the page text will be cached until
  // these conditions are met.
  string16 classifier_page_text_;

  DISALLOW_COPY_AND_ASSIGN(PhishingClassifierDelegate);
};

}  // namespace safe_browsing

#endif  // CHROME_RENDERER_SAFE_BROWSING_PHISHING_CLASSIFIER_DELEGATE_H_
