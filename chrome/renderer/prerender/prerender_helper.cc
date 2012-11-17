// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/prerender/prerender_helper.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "chrome/common/prerender_messages.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using content::DocumentState;

namespace {

// Updates the visibility state of the RenderView.  Must be called whenever
// prerendering starts or finishes and the page is about to be show.  At both
// those times, the RenderView is hidden.
void UpdateVisibilityState(content::RenderView* render_view) {
  if (render_view->GetWebView()) {
    render_view->GetWebView()->setVisibilityState(
        render_view->GetVisibilityState(), false);
  }
}

}  // namespace

namespace prerender {

PrerenderHelper::PrerenderHelper(content::RenderView* render_view)
    : content::RenderViewObserver(render_view),
      content::RenderViewObserverTracker<PrerenderHelper>(render_view) {
  UpdateVisibilityState(render_view);
}

PrerenderHelper::~PrerenderHelper() {
}

// static.
bool PrerenderHelper::IsPrerendering(const content::RenderView* render_view) {
  return PrerenderHelper::Get(render_view) != NULL;
}

bool PrerenderHelper::OnMessageReceived(
    const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(PrerenderHelper, message)
    IPC_MESSAGE_HANDLER(PrerenderMsg_SetIsPrerendering, OnSetIsPrerendering)
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

  content::RenderView* view = render_view();
  // |this| must be deleted so PrerenderHelper::IsPrerendering returns false
  // when the visibility state is updated, so the visibility state string will
  // not be "prerendered".
  delete this;

  UpdateVisibilityState(view);
}

}  // namespace prerender
