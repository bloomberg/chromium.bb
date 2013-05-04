// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/render_process_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/test/mock_render_process.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class RenderThreadImplBrowserTest : public testing::Test {
 public:
  virtual ~RenderThreadImplBrowserTest() {}
};

class DummyListener : public IPC::Listener {
 public:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    return true;
  }
};

void CheckRenderThreadInputHandlerManager(RenderThreadImpl* thread) {
  ASSERT_TRUE(thread->input_handler_manager());
}

// Check that InputHandlerManager outlives compositor thread because it uses
// raw pointers to post tasks.
TEST_F(RenderThreadImplBrowserTest,
    InputHandlerManagerDestroyedAfterCompositorThread) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableThreadedCompositing);

  ContentRendererClient content_renderer_client;
  SetRendererClientForTesting(&content_renderer_client);
  base::MessageLoopForIO message_loop_;

  std::string channel_id = IPC::Channel::GenerateVerifiedChannelID(
      std::string());
  DummyListener dummy_listener;
  IPC::Channel channel(channel_id, IPC::Channel::MODE_SERVER, &dummy_listener);
  ASSERT_TRUE(channel.Connect());

  scoped_ptr<MockRenderProcess> mock_process(new MockRenderProcess);
  // Owned by mock_process.
  RenderThreadImpl* thread = new RenderThreadImpl(channel_id);
  thread->EnsureWebKitInitialized();

  ASSERT_TRUE(thread->input_handler_manager());

  thread->compositor_message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&CheckRenderThreadInputHandlerManager, thread));
}

}  // namespace
}  // namespace content
