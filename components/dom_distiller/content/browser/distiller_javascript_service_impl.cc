// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/distiller_javascript_service_impl.h"
#include "components/dom_distiller/content/browser/external_feedback_reporter.h"
#include "components/dom_distiller/core/feedback_reporter.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/string.h"

namespace dom_distiller {

DistillerJavaScriptServiceImpl::DistillerJavaScriptServiceImpl(
    content::RenderFrameHost* render_frame_host,
    ExternalFeedbackReporter* external_feedback_reporter,
    mojo::InterfaceRequest<DistillerJavaScriptService> request)
    : binding_(this, request.Pass()),
      render_frame_host_(render_frame_host),
      external_feedback_reporter_(external_feedback_reporter) {}

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
  if (!external_feedback_reporter_) {
    return;
  }
  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  external_feedback_reporter_->ReportExternalFeedback(
      contents, contents->GetURL(), false);
  return;
}

void CreateDistillerJavaScriptService(
    content::RenderFrameHost* render_frame_host,
    ExternalFeedbackReporter* feedback_reporter,
    mojo::InterfaceRequest<DistillerJavaScriptService> request) {
  // This is strongly bound and owned by the pipe.
  new DistillerJavaScriptServiceImpl(render_frame_host, feedback_reporter,
      request.Pass());
}

}  // namespace dom_distiller
