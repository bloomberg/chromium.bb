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


static GURL StripRef(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

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
      last_finished_load_id_(-1),
      last_page_id_sent_to_classifier_(-1) {
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
  // Start classifying the current page if all conditions are met.
  // See MaybeStartClassification() for details.
  MaybeStartClassification();
}


void PhishingClassifierDelegate::OnStartPhishingDetection(const GURL& url) {
  last_url_received_from_browser_ = StripRef(url);
  // Start classifying the current page if all conditions are met.
  // See MaybeStartClassification() for details.
  MaybeStartClassification();
}

void PhishingClassifierDelegate::DidCommitProvisionalLoad(
    WebKit::WebFrame* frame, bool is_new_navigation) {
  // A new page is starting to load.  Unless the load is a navigation within
  // the same page, we need to cancel classification since we may get an
  // inconsistent result.
  NavigationState* state = NavigationState::FromDataSource(
      frame->dataSource());
  if (!state->was_within_same_page()) {
    CancelPendingClassification();
  }
}

void PhishingClassifierDelegate::PageCaptured(const string16& page_text) {
  last_finished_load_id_ = render_view()->page_id();
  last_finished_load_url_ = StripToplevelUrl();
  classifier_page_text_ = page_text;
  MaybeStartClassification();
}

void PhishingClassifierDelegate::CancelPendingClassification() {
  if (classifier_->is_ready()) {
    classifier_->CancelPendingClassification();
  }
  classifier_page_text_.clear();
}

bool PhishingClassifierDelegate::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PhishingClassifierDelegate, message)
    IPC_MESSAGE_HANDLER(ViewMsg_StartPhishingDetection,
                        OnStartPhishingDetection)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
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
  return StripRef(render_view()->webview()->mainFrame()->url());
}

void PhishingClassifierDelegate::MaybeStartClassification() {
  // We can begin phishing classification when the following conditions are
  // met:
  //  1. A Scorer has been created
  //  2. The browser has sent a StartPhishingDetection message for the current
  //     toplevel URL.
  //  3. The page has finished loading and the page text has been extracted.
  //  4. The load is a new navigation (not a session history navigation).
  //  5. The toplevel URL has not already been classified.
  //
  // Note that if we determine that this particular navigation should not be
  // classified at all (as opposed to deferring it until we get an IPC or the
  // load completes), we discard the page text since it won't be needed.
  if (!classifier_->is_ready()) {
    VLOG(2) << "Not starting classification, no Scorer created.";
    // Keep classifier_page_text_, in case a Scorer is set later.
    return;
  }

  if (last_finished_load_id_ <= last_page_id_sent_to_classifier_) {
    // Skip loads from session history navigation.
    VLOG(2) << "Not starting classification, last finished load id is "
            << last_finished_load_id_ << " but we have classified up to "
            << "load id " << last_page_id_sent_to_classifier_;
    classifier_page_text_.clear();  // we won't need this.
    return;
  }

  if (last_finished_load_id_ != render_view()->page_id()) {
    VLOG(2) << "Render view page has changed, not starting classification";
    classifier_page_text_.clear();  // we won't need this.
    return;
  }
  // If the page id is unchanged, the toplevel URL should also be unchanged.
  DCHECK_EQ(StripToplevelUrl(), last_finished_load_url_);

  if (last_finished_load_url_ == last_url_sent_to_classifier_) {
    // We've already classified this toplevel URL, so this was likely an
    // in-page navigation or a subframe navigation.  The browser should not
    // send a StartPhishingDetection IPC in this case.
    VLOG(2) << "Toplevel URL is unchanged, not starting classification.";
    classifier_page_text_.clear();  // we won't need this.
    return;
  }

  if (last_url_received_from_browser_ != last_finished_load_url_) {
    // The browser has not yet confirmed that this URL should be classified,
    // so defer classification for now.
    VLOG(2) << "Not starting classification, last url from browser is "
            << last_url_received_from_browser_ << ", last finished load is "
            << last_finished_load_url_;
    // Keep classifier_page_text_, in case the browser notifies us later that
    // we should classify the URL.
    return;
  }

  VLOG(2) << "Starting classification for " << last_finished_load_url_;
  last_url_sent_to_classifier_ = last_finished_load_url_;
  last_page_id_sent_to_classifier_ = last_finished_load_id_;
  classifier_->BeginClassification(
      &classifier_page_text_,
      NewCallback(this, &PhishingClassifierDelegate::ClassificationDone));
}

}  // namespace safe_browsing
