// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_RENDERER_RENDERER_CLD_DATA_PROVIDER_H_
#define COMPONENTS_TRANSLATE_CONTENT_RENDERER_RENDERER_CLD_DATA_PROVIDER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "ipc/ipc_listener.h"

namespace IPC {
class Message;
}

namespace translate {

// Renderer-side interface responsible for providing CLD data.
// The embedder should set an instance as soon as feasible during startup.
// The implementation must be paired with a browser-side implementation of
// the BrowserCldDataProvider class, typically created by a
// BrowserCldDataProviderFactory:
//   ../browser/browser_cld_data_provider_factory.h
//   ../browser/browser_cld_data_provider.h
//
// The glue between the browser and renderer processes is typically a pair of
// request/response IPC messages using the CldDataProviderMsgStart
// "IPCMessageStart" enumerated constant from ipc_message_start.h.
class RendererCldDataProvider : public IPC::Listener {
 public:
  RendererCldDataProvider() {}
  ~RendererCldDataProvider() override {}

  // (Inherited from IPC::Listener)
  // If the specified message is a response for CLD data, attempts to
  // initialize CLD2 and returns true in all cases. If initialization is
  // successful and a callback has been configured via
  // SetCldAvailableCallback(...), that callback is invoked from the message
  // loop thread.
  // This method is defined as virtual in order to force the implementation to
  // define the specific IPC message(s) that it handles.
  // The default implementation does nothing and returns false.
  virtual bool OnMessageReceived(const IPC::Message& message) override;

  // Invoked by the renderer process to request that CLD data be obtained and
  // that CLD be initialized with it. The implementation is expected to
  // communicate with the paired BrowserCldDataProvider implementation on the
  // browser side.
  // This method must be invoked on the message loop thread.
  // The default implementation does nothing.
  virtual void SendCldDataRequest() {}

  // Convenience method that tracks whether or not CLD data is available.
  // This method can be used in the absence of a callback (i.e., if the caller
  // wants a simple way to check the state of CLD data availability without
  // keeping a separate boolean flag tripped by a callback).
  // The default implementation always returns true.
  virtual bool IsCldDataAvailable();

  // Sets a callback that will be invoked when CLD data is successfully
  // obtained from the paired BrowserCldDataProvider implementation on the
  // browser side, after CLD has been successfully initialized.
  // Both the initialization of CLD2 as well as the invocation of the callback
  // must happen on the message loop thread.
  // The default implementation immediately invokes the callback.
  virtual void SetCldAvailableCallback(base::Callback<void(void)> callback);

  // Sets the default data provider for this process, i.e. the provider to be
  // used unless the embedder calls Set(RendererCldDataProvider*). This is the
  // method that normal (i.e., non-test) Chromium code should use; embedders can
  // and should use the unconditional Set(RendererCldDataProvider*) method
  // instead. If a default provider has already been set, this method does
  // nothing.
  static void SetDefault(RendererCldDataProvider* instance);

  // Unconditionally sets the data provider for this process, overwriting any
  // previously-configured default. Normal Chromium code should never use this
  // method; it is provided for embedders to inject a provider from outside of
  // the Chromium code base. Test code can also use this method to force the
  // runtime to have a desired behavior.
  //
  // The caller is responsible for the lifecycle of the instance. In general,
  // the instance passed here should live throughout the lifetime of the
  // process.
  static void Set(RendererCldDataProvider* instance);

  // Returns true if and only if the current instance for this process is not
  // NULL.
  static bool IsInitialized();

  // Returns the instance of the provider previously set by Set(...).
  // If no instance has been set, a default no-op provider will be returned.
  static RendererCldDataProvider* Get();

 private:
  DISALLOW_COPY_AND_ASSIGN(RendererCldDataProvider);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_RENDERER_RENDERER_CLD_DATAP_PROVIDER_H_
