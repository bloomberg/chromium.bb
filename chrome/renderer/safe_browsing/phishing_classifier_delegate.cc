// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/phishing_classifier_delegate.h"

#include <set>

#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/scoped_callback_factory.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/navigation_state.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/safe_browsing/feature_extractor_clock.h"
#include "chrome/renderer/safe_browsing/phishing_classifier.h"
#include "chrome/renderer/safe_browsing/scorer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

namespace safe_browsing {

typedef std::set<PhishingClassifierDelegate*> PhishingClassifierDelegates;
static base::LazyInstance<PhishingClassifierDelegates>
    g_delegates(base::LINKER_INITIALIZED);

static base::LazyInstance<scoped_ptr<const safe_browsing::Scorer> >
    g_phishing_scorer(base::LINKER_INITIALIZED);

class ScorerCallback {
 public:
  static Scorer::CreationCallback* CreateCallback() {
    ScorerCallback* scorer_callback = new ScorerCallback();
    return scorer_callback->callback_factory_->NewCallback(
        &ScorerCallback::PhishingScorerCreated);
  }

 private:
  ScorerCallback() {
    callback_factory_.reset(
        new base::ScopedCallbackFactory<ScorerCallback>(this));
  }

  // Callback to be run once the phishing Scorer has been created.
  void PhishingScorerCreated(safe_browsing::Scorer* scorer) {
    if (!scorer) {
      DLOG(ERROR) << "Unable to create a PhishingScorer - corrupt model?";
      return;
    }

    g_phishing_scorer.Get().reset(scorer);

    PhishingClassifierDelegates::iterator i;
    for (i = g_delegates.Get().begin(); i != g_delegates.Get().end(); ++i)
      (*i)->SetPhishingScorer(scorer);

    delete this;
  }

  scoped_ptr<base::ScopedCallbackFactory<ScorerCallback> > callback_factory_;
};

void PhishingClassifierDelegate::SetPhishingModel(
    IPC::PlatformFileForTransit model_file) {
  safe_browsing::Scorer::CreateFromFile(
      IPC::PlatformFileForTransitToPlatformFile(model_file),
      RenderThread::current()->GetFileThreadMessageLoopProxy(),
      ScorerCallback::CreateCallback());
}

PhishingClassifierDelegate::PhishingClassifierDelegate(
    RenderView* render_view,
    PhishingClassifier* classifier)
    : RenderViewObserver(render_view),
      last_page_id_sent_to_classifier_(-1),
      pending_classification_(false) {
  g_delegates.Get().insert(this);
  if (!classifier) {
    classifier = new PhishingClassifier(render_view,
                                        new FeatureExtractorClock());
  }

  classifier_.reset(classifier);

  if (g_phishing_scorer.Get().get())
    SetPhishingScorer(g_phishing_scorer.Get().get());
}

PhishingClassifierDelegate::~PhishingClassifierDelegate() {
  CancelPendingClassification();
  g_delegates.Get().erase(this);
}

void PhishingClassifierDelegate::SetPhishingScorer(
    const safe_browsing::Scorer* scorer) {
  if (!render_view()->webview())
    return;  // RenderView is tearing down.

  classifier_->set_phishing_scorer(scorer);

  if (pending_classification_) {
    pending_classification_ = false;
    // If we have a pending classificaton, it should always be true that the
    // main frame URL and page id have not changed since we queued the
    // classification.  This is because we stop any pending classification on
    // main frame loads in RenderView::didCommitProvisionalLoad().
    DCHECK_EQ(StripToplevelUrl(), last_url_sent_to_classifier_);
    DCHECK_EQ(render_view()->page_id(), last_page_id_sent_to_classifier_);
    classifier_->BeginClassification(
        &classifier_page_text_,
        NewCallback(this, &PhishingClassifierDelegate::ClassificationDone));
  }
}

void PhishingClassifierDelegate::DidCommitProvisionalLoad(
    WebKit::WebFrame* frame, bool is_new_navigation) {
  if (!is_new_navigation)
    return;

  // A new page is starting to load.  Unless the load is a navigation within
  // the same page, we need to cancel classification since the content will
  // now be inconsistent with the phishing model.
  NavigationState* state = NavigationState::FromDataSource(
      frame->dataSource());
  if (!state->was_within_same_page()) {
    CancelPendingClassification();
  }
}

void PhishingClassifierDelegate::PageCaptured(const string16& page_text) {
  // We check that the page id has incremented so that we don't reclassify
  // pages as the user moves back and forward in session history.  Note: we
  // don't send every page id to the classifier, only those where the toplevel
  // URL changed.
  int load_id = render_view()->page_id();
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
  classifier_page_text_ = page_text;

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

bool PhishingClassifierDelegate::OnMessageReceived(
    const IPC::Message& message) {
  /*
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PhishingClassifierDelegate, message)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
  */
  return false;
}

void PhishingClassifierDelegate::ClassificationDone(bool is_phishy,
                                                    double phishy_score) {
  // We no longer need the page text.
  classifier_page_text_.clear();
  VLOG(2) << "Phishy verdict = " << is_phishy << " score = " << phishy_score;
  if (!is_phishy) {
    return;
  }

  render_view()->Send(new ViewHostMsg_DetectedPhishingSite(
      render_view()->routing_id(),
      last_url_sent_to_classifier_,
      phishy_score));
}

GURL PhishingClassifierDelegate::StripToplevelUrl() {
  GURL toplevel_url = render_view()->webview()->mainFrame()->url();
  GURL::Replacements replacements;
  replacements.ClearRef();
  return toplevel_url.ReplaceComponents(replacements);
}

}  // namespace safe_browsing
