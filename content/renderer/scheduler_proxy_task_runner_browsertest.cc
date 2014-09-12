// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/render_process_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/scheduler_proxy_task_runner.h"
#include "content/test/mock_render_process.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

void TestTask(int value, int* result) {
  *result = (*result << 4) | value;
}

}  // namespace

class DummyListener : public IPC::Listener {
 public:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    return true;
  }
};

TEST(SchedulerProxyTaskRunnerBrowserTest, TestTaskPosting) {
  ContentClient content_client;
  ContentBrowserClient content_browser_client;
  ContentRendererClient content_renderer_client;
  SetContentClient(&content_client);
  SetBrowserClientForTesting(&content_browser_client);
  SetRendererClientForTesting(&content_renderer_client);
  base::MessageLoopForIO message_loop;

  std::string channel_id =
      IPC::Channel::GenerateVerifiedChannelID(std::string());
  DummyListener dummy_listener;
  scoped_ptr<IPC::Channel> channel(
      IPC::Channel::CreateServer(channel_id, &dummy_listener));
  EXPECT_TRUE(channel->Connect());

  scoped_ptr<MockRenderProcess> mock_process(new MockRenderProcess);
  // Owned by mock_process.
  RenderThreadImpl* thread = new RenderThreadImpl(channel_id);
  thread->EnsureWebKitInitialized();

  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner =
      thread->main_thread_compositor_task_runner();

  EXPECT_TRUE(compositor_task_runner->BelongsToCurrentThread());

  int compositor_order = 0;
  compositor_task_runner->PostTask(FROM_HERE,
                                   base::Bind(&TestTask, 1, &compositor_order));
  compositor_task_runner->PostTask(FROM_HERE,
                                   base::Bind(&TestTask, 2, &compositor_order));
  compositor_task_runner->PostTask(FROM_HERE,
                                   base::Bind(&TestTask, 3, &compositor_order));
  compositor_task_runner->PostTask(FROM_HERE,
                                   base::Bind(&TestTask, 4, &compositor_order));

  message_loop.RunUntilIdle();

  EXPECT_EQ(0x1234, compositor_order);
}

}  // namespace content
