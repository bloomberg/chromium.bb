// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_web_contents_observer.h"

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

ThumbnailWebContentsObserver::ThumbnailWebContentsObserver(
    content::WebContents* contents)
    : content::WebContentsObserver(contents) {}

ThumbnailWebContentsObserver::~ThumbnailWebContentsObserver() = default;

void ThumbnailWebContentsObserver::OnVisibilityChanged(
    content::Visibility visibility) {
  VisibilityChanged(visibility == content::Visibility::VISIBLE);
}

void ThumbnailWebContentsObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame() &&
      !navigation_handle->IsSameDocument()) {
    TopLevelNavigationStarted(
        navigation_handle->GetWebContents()->GetVisibleURL());
  }
}

void ThumbnailWebContentsObserver::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame() &&
      !navigation_handle->IsSameDocument()) {
    TopLevelNavigationStarted(
        navigation_handle->GetWebContents()->GetVisibleURL());
  }
}

void ThumbnailWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame()) {
    TopLevelNavigationEnded(
        navigation_handle->GetWebContents()->GetVisibleURL());
  }
}

void ThumbnailWebContentsObserver::DidStartLoading() {
  PageLoadStarted(FrameContext::kMainFrame);
}

void ThumbnailWebContentsObserver::DidStopLoading() {
  PageLoadFinished(FrameContext::kMainFrame);
}

void ThumbnailWebContentsObserver::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  PageLoadFinished(ContextFromRenderFrameHost(render_frame_host));
}

void ThumbnailWebContentsObserver::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  PageLoadFinished(ContextFromRenderFrameHost(render_frame_host));
}

void ThumbnailWebContentsObserver::MainFrameWasResized(bool width_changed) {
  PageUpdated(FrameContext::kMainFrame);
}

void ThumbnailWebContentsObserver::FrameSizeChanged(
    content::RenderFrameHost* render_frame_host,
    const gfx::Size& frame_size) {
  PageUpdated(ContextFromRenderFrameHost(render_frame_host));
}

void ThumbnailWebContentsObserver::NavigationStopped() {
  TopLevelNavigationEnded(web_contents()->GetVisibleURL());
  PageLoadFinished(FrameContext::kMainFrame);
}

// static
ThumbnailWebContentsObserver::FrameContext
ThumbnailWebContentsObserver::ContextFromRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  return render_frame_host->GetParent() ? FrameContext::kChildFrame
                                        : FrameContext::kMainFrame;
}
