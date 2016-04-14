// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_RENDER_THREAD_IMPL_BROWSER_TEST_IPC_HELPER_H_
#define CONTENT_TEST_RENDER_THREAD_IMPL_BROWSER_TEST_IPC_HELPER_H_

#include "content/app/mojo/mojo_init.h"
#include "content/browser/mojo/mojo_application_host.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/mojo/ipc_channel_mojo.h"
#include "ipc/mojo/scoped_ipc_support.h"

namespace IPC {
class ChannelProxy;
class Sender;
};

namespace content {

// Helper for RenderThreadImplBrowserTest which takes care of setting up an IPC
// server, capable of sending messages to the RenderThread.
class RenderThreadImplBrowserIPCTestHelper {
 public:
  RenderThreadImplBrowserIPCTestHelper();
  ~RenderThreadImplBrowserIPCTestHelper();

  IPC::Sender* Sender() const { return channel_.get(); }

  base::MessageLoop* GetMessageLoop() const { return message_loop_.get(); }

  const std::string& GetChannelId() const { return channel_id_; }

  scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() const;

  const std::string& GetMojoIpcToken() const {
    return mojo_ipc_token_;
  }

  const std::string& GetMojoApplicationToken() const {
    return mojo_application_token_;
  }

 private:
  class DummyListener;

  void SetupIpcThread();
  void SetupMojo();

  std::unique_ptr<IPC::ChannelProxy> channel_;
  std::unique_ptr<base::Thread> ipc_thread_;
  std::unique_ptr<base::MessageLoopForIO> message_loop_;
  std::unique_ptr<DummyListener> dummy_listener_;
  std::unique_ptr<IPC::ScopedIPCSupport> ipc_support_;
  std::unique_ptr<MojoApplicationHost> mojo_application_host_;
  std::string mojo_ipc_token_;
  std::string mojo_application_token_;
  std::string channel_id_;
};

}  // namespace content

#endif  // CONTENT_TEST_RENDER_THREAD_IMPL_BROWSER_TEST_IPC_HELPER_H_
