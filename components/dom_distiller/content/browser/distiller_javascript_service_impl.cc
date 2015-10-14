// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/distiller_javascript_service_impl.h"
#include "components/dom_distiller/content/browser/distiller_ui_handle.h"
#include "components/dom_distiller/core/feedback_reporter.h"
#include "content/public/browser/user_metrics.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/string.h"

namespace dom_distiller {

DistillerJavaScriptServiceImpl::DistillerJavaScriptServiceImpl(
    content::RenderFrameHost* render_frame_host,
    DistillerUIHandle* distiller_ui_handle,
    mojo::InterfaceRequest<DistillerJavaScriptService> request)
    : binding_(this, request.Pass()),
      render_frame_host_(render_frame_host),
      distiller_ui_handle_(distiller_ui_handle) {}

DistillerJavaScriptServiceImpl::~DistillerJavaScriptServiceImpl() {}

void DistillerJavaScriptServiceImpl::HandleDistillerEchoCall(
    const mojo::String& message) {}

void DistillerJavaScriptServiceImpl::HandleDistillerFeedbackCall(
    bool good) {
  FeedbackReporter::ReportQuality(good);
  if (good) {
    return;
  }

  // If feedback is bad try to start up external feedback.
  if (!distiller_ui_handle_) {
    return;
  }
  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  distiller_ui_handle_->ReportExternalFeedback(
      contents, contents->GetURL(), false);
  return;
}

void DistillerJavaScriptServiceImpl::HandleDistillerClosePanelCall() {
  content::RecordAction(base::UserMetricsAction("DomDistiller_ViewOriginal"));
}

void DistillerJavaScriptServiceImpl::HandleDistillerOpenSettingsCall() {
  if (!distiller_ui_handle_) {
    return;
  }
  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  distiller_ui_handle_->OpenSettings(contents);
}

void CreateDistillerJavaScriptService(
    content::RenderFrameHost* render_frame_host,
    DistillerUIHandle* distiller_ui_handle,
    mojo::InterfaceRequest<DistillerJavaScriptService> request) {
  // This is strongly bound and owned by the pipe.
  new DistillerJavaScriptServiceImpl(render_frame_host, distiller_ui_handle,
      request.Pass());
}

}  // namespace dom_distiller
