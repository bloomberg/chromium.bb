// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLER_JAVASCRIPT_SERVICE_IMPL_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLER_JAVASCRIPT_SERVICE_IMPL_H_

#include "components/dom_distiller/content/browser/external_feedback_reporter.h"
#include "components/dom_distiller/content/common/distiller_javascript_service.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/string.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"

namespace dom_distiller {

// This is the receiving end of "distiller" JavaScript object calls.
class DistillerJavaScriptServiceImpl : public DistillerJavaScriptService {
 public:
  DistillerJavaScriptServiceImpl(
      content::RenderFrameHost* render_frame_host,
      ExternalFeedbackReporter* external_feedback_reporter,
      mojo::InterfaceRequest<DistillerJavaScriptService> request);
  ~DistillerJavaScriptServiceImpl() override;

  // Mojo DistillerJavaScriptService implementation.

  // Echo implementation, this call does not actually return as it would be
  // blocking.
  void HandleDistillerEchoCall(const mojo::String& message) override;

  // Send UMA feedback and start the external feedback reporter if one exists.
  void HandleDistillerFeedbackCall(bool good) override;

 private:
  mojo::StrongBinding<DistillerJavaScriptService> binding_;
  content::RenderFrameHost* render_frame_host_;
  ExternalFeedbackReporter* external_feedback_reporter_;
};

// static
void CreateDistillerJavaScriptService(
    content::RenderFrameHost* render_frame_host,
    ExternalFeedbackReporter* feedback_reporter,
    mojo::InterfaceRequest<DistillerJavaScriptService> request);

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLER_JAVASCRIPT_SERVICE_IMPL_H_
