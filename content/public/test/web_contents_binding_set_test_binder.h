// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents_binding_set.h"

#include "mojo/public/cpp/bindings/associated_interface_request.h"

namespace content {

// Helper class which test code can use with
// WebContentsImpl::OverrideBinderForTesting() in order to override
// WebContentsBindingSet interface binding behavior in tests.
template <typename Interface>
class WebContentsBindingSetTestBinder : public WebContentsBindingSet::Binder {
 public:
  ~WebContentsBindingSetTestBinder() override {}

  // Call for every new incoming interface request for a frame.
  virtual void BindRequest(
      RenderFrameHost* render_frame_host,
      mojo::AssociatedInterfaceRequest<Interface> request) = 0;

 private:
  // Binder:
  void OnRequestForFrame(RenderFrameHost* render_frame_host,
                         mojo::ScopedInterfaceEndpointHandle handle) override {
    mojo::AssociatedInterfaceRequest<Interface> request;
    request.Bind(std::move(handle));
    BindRequest(render_frame_host, std::move(request));
  }
};

}  // namespace content
