// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/web_contents_main_frame_observer.h"

#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(dom_distiller::WebContentsMainFrameObserver);

namespace dom_distiller {

WebContentsMainFrameObserver::WebContentsMainFrameObserver(
    content::WebContents* web_contents)
    : is_document_loaded_in_main_frame_(false),
      is_initialized_(false),
      web_contents_(web_contents) {
  content::WebContentsObserver::Observe(web_contents);
}

WebContentsMainFrameObserver::~WebContentsMainFrameObserver() {
  CleanUp();
}

void WebContentsMainFrameObserver::DocumentLoadedInFrame(
    int64 frame_id,
    content::RenderViewHost* render_view_host) {
  if (web_contents_ &&
      frame_id == web_contents_->GetMainFrame()->GetRoutingID()) {
    is_document_loaded_in_main_frame_ = true;
  }
}

void WebContentsMainFrameObserver::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (details.is_main_frame) {
    is_document_loaded_in_main_frame_ = false;
    is_initialized_ = true;
  }
}

void WebContentsMainFrameObserver::RenderProcessGone(
    base::TerminationStatus status) {
  CleanUp();
}

void WebContentsMainFrameObserver::WebContentsDestroyed() {
  CleanUp();
}

void WebContentsMainFrameObserver::CleanUp() {
  content::WebContentsObserver::Observe(NULL);
  web_contents_ = NULL;
}

}  // namespace dom_distiller
