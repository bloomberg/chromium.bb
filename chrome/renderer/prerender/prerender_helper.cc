// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/prerender/prerender_helper.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "chrome/common/render_messages.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using content::DocumentState;

// Helper macro for histograms.
#define RECORD_PLT(tag, perceived_page_load_time) { \
    UMA_HISTOGRAM_CUSTOM_TIMES( \
        base::FieldTrial::MakeName(std::string("Prerender.") + tag, \
                                   "Prefetch"), \
        perceived_page_load_time, \
        base::TimeDelta::FromMilliseconds(10), \
        base::TimeDelta::FromSeconds(60), \
        100); \
  }

namespace prerender {

PrerenderHelper::PrerenderHelper(content::RenderView* render_view)
    : content::RenderViewObserver(render_view),
      content::RenderViewObserverTracker<PrerenderHelper>(render_view),
      is_prerendering_(true),
      has_unrecorded_data_(false) {
  UpdateVisibilityState();
}

PrerenderHelper::~PrerenderHelper() {
}

// static.
bool PrerenderHelper::IsPrerendering(const content::RenderView* render_view) {
  PrerenderHelper* prerender_helper = PrerenderHelper::Get(render_view);
  return (prerender_helper && prerender_helper->is_prerendering_);
}

// static.
void PrerenderHelper::RecordHistograms(
    content::RenderView* render_view,
    const base::Time& finish_all_loads,
    const base::TimeDelta& begin_to_finish_all_loads) {
  static bool use_prerender_histogram =
      base::FieldTrialList::TrialExists("Prefetch");
  if (!use_prerender_histogram)
    return;

  PrerenderHelper* prerender_helper = PrerenderHelper::Get(render_view);

  // Load time for non-prerendered pages.
  if (!prerender_helper) {
    RECORD_PLT("RendererPerceivedPLT", begin_to_finish_all_loads);
    return;
  }

  if (!prerender_helper->is_prerendering_ &&
      !prerender_helper->HasUnrecordedData()) {
    // If the RenderView has a PrerenderHelper and a histogram is being
    // recorded, it should either be prerendering or have histogram data that
    // has yet to be recorded.
    NOTREACHED();
    delete prerender_helper;
    return;
  }

  // There should be a start time, since the first provisional load should have
  // occured before recording any histograms.
  DCHECK(!prerender_helper->prerender_start_time_.is_null());

  // The only case where this should happen is if a page is being redirected
  // prior to display.  No histograms are currently recorded when the renderer
  // is shutting down, so this point will never be reached in that case.
  if (prerender_helper->is_prerendering_) {
    DCHECK(!prerender_helper->HasUnrecordedData());
    return;
  }

  // There should be a display time, since HasUnrecordedData() returned true.
  DCHECK(!prerender_helper->prerender_display_time_.is_null());

  // The RenderView still has a PrerenderHelper and is not currently being
  // prerendered, so the page was prerendered and then displayed.  Record
  // histograms for the prerender, before deleting the PrerenderHelper.
  base::Time prerender_display_time =
      prerender_helper->prerender_display_time_;
  base::Time prerender_start_time = prerender_helper->prerender_start_time_;

  RECORD_PLT("RendererTimeUntilDisplayed",
             prerender_display_time - prerender_start_time);
  base::TimeDelta perceived_load_time = finish_all_loads -
                                        prerender_display_time;
  if (perceived_load_time < base::TimeDelta::FromSeconds(0)) {
    RECORD_PLT("RendererIdleTime", -perceived_load_time);
    perceived_load_time = base::TimeDelta::FromSeconds(0);
  }
  RECORD_PLT("RendererPerceivedPLT", perceived_load_time);
  RECORD_PLT("RendererPerceivedPLTMatched", perceived_load_time);

  // Once a prerendered page is displayed and its histograms recorded, it no
  // longer needs a PrerenderHelper.
  delete prerender_helper;
}

void PrerenderHelper::WillCreateMediaPlayer(
    WebKit::WebFrame* frame,
    WebKit::WebMediaPlayerClient* client) {
  if (is_prerendering_) {
    // Cancel prerendering in the case of HTML5 media, to avoid playing sounds
    // in the background.
    Send(new ChromeViewHostMsg_MaybeCancelPrerenderForHTML5Media(
        render_view()->GetRoutingId()));
  }
}

void PrerenderHelper::DidStartProvisionalLoad(WebKit::WebFrame* frame) {
  // If this is the first provisional load since prerendering started, get its
  // request time.
  if (is_prerendering_ && prerender_start_time_.is_null()) {
    WebKit::WebDataSource* data_source = frame->provisionalDataSource();
    if (!data_source) {
      NOTREACHED();
      return;
    }
    DocumentState* document_state =
        DocumentState::FromDataSource(data_source);
    prerender_start_time_ = document_state->request_time();
    // The first navigation for prerendering RenderViews can only be triggered
    // from PrerenderContents, so there should be a request_time.
    DCHECK(!prerender_start_time_.is_null());
  }
}

bool PrerenderHelper::OnMessageReceived(
    const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(PrerenderHelper, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetIsPrerendering, OnSetIsPrerendering)
  IPC_END_MESSAGE_MAP()
  // Return false on ViewMsg_SetIsPrerendering so other observers can see the
  // message.
  return false;
}

void PrerenderHelper::OnSetIsPrerendering(bool is_prerendering) {
  // Immediately after construction, |this| may receive the message that
  // triggered its creation.  If so, ignore it.
  if (is_prerendering)
    return;
  DCHECK(!is_prerendering);
  DCHECK(!HasUnrecordedData());

  is_prerendering_ = false;
  prerender_display_time_ = base::Time::Now();
  UpdateVisibilityState();
}

bool PrerenderHelper::HasUnrecordedData() const {
  return !prerender_display_time_.is_null();
}

void PrerenderHelper::UpdateVisibilityState() {
  if (render_view()->GetWebView()) {
    render_view()->GetWebView()->setVisibilityState(
        render_view()->GetVisibilityState(), false);
  }
}

}  // namespace prerender
