// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_BROWSER_BROWSER_CLD_DATA_PROVIDER_H_
#define COMPONENTS_TRANSLATE_CONTENT_BROWSER_BROWSER_CLD_DATA_PROVIDER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ipc/ipc_listener.h"

namespace IPC {
class Message;
}

namespace translate {

// Browser-side interface responsible for providing CLD data.
// The implementation must be paired with a renderer-side implementation of
// the RendererCldDataProvider class:
//   ../renderer/renderer_cld_data_provider.h
//
// The glue between them is typically a pair of request/response IPC messages
// using the "CldDataProviderMsgStart" IPCMessageStart enumerated constant from
// ipc_message_start.h
//
// In general, instances of this class should be obtained by using the
// BrowserCldDataProviderFactory::CreateBrowserCldProvider(...). For more
// information, see browser_cld_data_provider_factory.h.
class BrowserCldDataProvider : public IPC::Listener {
 public:
  BrowserCldDataProvider() {}
  ~BrowserCldDataProvider() override {}

  // IPC::Listener implementation:
  // If the specified message is a request for CLD data, invokes
  // OnCldDataRequest() and returns true. In all other cases, this method does
  // nothing. This method is defined as virtual in order to force the
  // implementation to define the specific IPC message(s) that it handles.
  // The default implementation does nothing and returns false.
  bool OnMessageReceived(const IPC::Message&) override;

  // Called when the browser process receives an appropriate message in
  // OnMessageReceived, above. The implementation should attempt to locate
  // the CLD data, cache any metadata required for accessing that data, and
  // ultimately trigger a response by invoking SendCldDataResponse.
  // The renderer process may poll for data, in which case this method may be
  // repeatedly invoked. The implementation must be safe to call any number
  // of times.
  // The default implementation does nothing.
  virtual void OnCldDataRequest() {}

  // Invoked when OnCldDataRequest, above, results in a successful lookup or
  // the data is already cached and ready to respond to. The implementation
  // should take whatever action is appropriate for responding to the paired
  // RendererCldDataProvider, typically by sending an IPC response.
  // The default implementation does nothing.
  virtual void SendCldDataResponse() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserCldDataProvider);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_BROWSER_BROWSER_CLD_DATA_PROVIDER_H_
