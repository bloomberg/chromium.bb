// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_RENDERER_RENDERER_CLD_DATA_PROVIDER_H_
#define COMPONENTS_TRANSLATE_CONTENT_RENDERER_RENDERER_CLD_DATA_PROVIDER_H_

#include "base/callback.h"
#include "ipc/ipc_listener.h"

namespace IPC {
class Message;
}

namespace content {
class RenderViewObserver;
}

namespace translate {

// Renderer-side interface responsible for providing CLD data.
// The implementation must be paired with a browser-side implementation of
// the BrowserCldDataProvider class:
//
//   components/translate/content/browser/browser_cld_data_provider.h
//
// ... and the glue between them is typically a pair of request/response IPC
// messages using the CldDataProviderMsgStart IPCMessageStart enumerated
// constant from ipc_message_start.h
class RendererCldDataProvider : public IPC::Listener {
 public:
  virtual ~RendererCldDataProvider() {}

  // (Inherited from IPC::Listener)
  // If the specified message is a response for CLD data, attempts to
  // initialize CLD2 and returns true in all cases. If initialization is
  // successful and a callback has been configured via
  // SetCldAvailableCallback(...), that callback is invoked from the message
  // loop thread.
  // This method is defined as virtual in order to force the implementation to
  // define the specific IPC message(s) that it handles.
  virtual bool OnMessageReceived(const IPC::Message&) = 0;

  // Invoked by the renderer process to request that CLD data be obtained and
  // that CLD be initialized with it. The implementation is expected to
  // communicate with the paired BrowserCldDataProvider implementation on the
  // browser side.
  // This method must be invoked on the message loop thread.
  virtual void SendCldDataRequest() = 0;

  // Convenience method that tracks whether or not CLD data is available.
  // This method can be used in the absence of a callback (i.e., if the caller
  // wants a simple way to check the state of CLD data availability without
  // keeping a separate boolean flag tripped by a callback).
  virtual bool IsCldDataAvailable() = 0;

  // Sets a callback that will be invoked when CLD data is successfully
  // obtained from the paired BrowserCldDataProvider implementation on the
  // browser side, after CLD has been successfully initialized.
  // Both the initialization of CLD2 as well as the invocation of the callback
  // must happen on the message loop thread.
  virtual void SetCldAvailableCallback(base::Callback<void(void)>) = 0;
};

// Static factory function defined by the implementation that produces a new
// provider for the specified render view host.
RendererCldDataProvider* CreateRendererCldDataProviderFor(
    content::RenderViewObserver*);

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_RENDERER_RENDERER_CLD_DATAP_PROVIDER_H_
