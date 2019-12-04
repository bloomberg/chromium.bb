// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_WEB_CONTENTS_RECEIVER_SET_TEST_BINDER_H_
#define CONTENT_PUBLIC_TEST_WEB_CONTENTS_RECEIVER_SET_TEST_BINDER_H_

#include "content/public/browser/web_contents_receiver_set.h"

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace content {

// Helper class which test code can use with
// WebContentsImpl::OverrideBinderForTesting() in order to override
// WebContentsReceiverSet interface receiver behavior in tests.
template <typename Interface>
class WebContentsReceiverSetTestBinder : public WebContentsReceiverSet::Binder {
 public:
  ~WebContentsReceiverSetTestBinder() override {}

  // Call for every new incoming receiver for a frame.
  virtual void BindReceiver(
      RenderFrameHost* render_frame_host,
      mojo::PendingAssociatedReceiver<Interface> receiver) = 0;

 private:
  // Binder:
  void OnReceiverForFrame(RenderFrameHost* render_frame_host,
                          mojo::ScopedInterfaceEndpointHandle handle) override {
    BindReceiver(render_frame_host,
                 mojo::PendingAssociatedReceiver<Interface>(std::move(handle)));
  }

  void CloseAllReceivers() override {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_WEB_CONTENTS_RECEIVER_SET_TEST_BINDER_H_
