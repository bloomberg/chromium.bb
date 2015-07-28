// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_web_contents_observer.h"

#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    offline_pages::OfflinePageWebContentsObserver);

namespace offline_pages {

OfflinePageWebContentsObserver::OfflinePageWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      is_document_loaded_in_main_frame_(false) {
}

OfflinePageWebContentsObserver::~OfflinePageWebContentsObserver() {
}

void OfflinePageWebContentsObserver::DocumentLoadedInFrame(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host->GetParent()) {
    is_document_loaded_in_main_frame_ = true;
    if (!main_frame_document_loaded_callback_.is_null())
      main_frame_document_loaded_callback_.Run();
  }
}

void OfflinePageWebContentsObserver::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (details.is_navigation_to_different_page())
    is_document_loaded_in_main_frame_ = false;
}

void OfflinePageWebContentsObserver::RenderProcessGone(
    base::TerminationStatus status) {
  CleanUp();
}

void OfflinePageWebContentsObserver::CleanUp() {
  content::WebContentsObserver::Observe(nullptr);
}

}  // namespace offline_pages
