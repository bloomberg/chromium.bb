// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/render_thread_impl_browser_test_ipc_helper.h"

#include "content/public/common/mojo_channel_switches.h"
#include "mojo/edk/embedder/embedder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class RenderThreadImplBrowserIPCTestHelper::DummyListener
    : public IPC::Listener {
 public:
  bool OnMessageReceived(const IPC::Message& message) override { return true; }
};

RenderThreadImplBrowserIPCTestHelper::RenderThreadImplBrowserIPCTestHelper() {
  message_loop_.reset(new base::MessageLoopForIO());

  channel_id_ = IPC::Channel::GenerateVerifiedChannelID("");
  dummy_listener_.reset(new DummyListener());

  SetupIpcThread();

  if (ShouldUseMojoChannel()) {
    SetupMojo();
  } else {
    channel_ = IPC::ChannelProxy::Create(channel_id_, IPC::Channel::MODE_SERVER,
                                         dummy_listener_.get(),
                                         ipc_thread_->task_runner());
  }
}

RenderThreadImplBrowserIPCTestHelper::~RenderThreadImplBrowserIPCTestHelper() {
}

void RenderThreadImplBrowserIPCTestHelper::SetupIpcThread() {
  ipc_thread_.reset(new base::Thread("test_ipc_thread"));
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  ASSERT_TRUE(ipc_thread_->StartWithOptions(options));
}

void RenderThreadImplBrowserIPCTestHelper::SetupMojo() {
  InitializeMojo();

  ipc_support_.reset(new IPC::ScopedIPCSupport(ipc_thread_->task_runner()));
  mojo_application_host_.reset(new MojoApplicationHost());
  mojo_application_token_ = mojo_application_host_->GetToken();

  mojo_ipc_token_ = mojo::edk::GenerateRandomToken();

  mojo::MessagePipe pipe;
  channel_ = IPC::ChannelProxy::Create(
      IPC::ChannelMojo::CreateServerFactory(
          mojo::edk::CreateParentMessagePipe(mojo_ipc_token_)),
      dummy_listener_.get(), ipc_thread_->task_runner());
}

scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImplBrowserIPCTestHelper::GetIOTaskRunner() const {
  return ipc_thread_->task_runner();
}

}  // namespace content
