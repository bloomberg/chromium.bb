// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_mock_render_thread.h"

#include "base/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(ENABLE_EXTENSIONS)
#include "extensions/common/extension_messages.h"
#endif

ChromeMockRenderThread::ChromeMockRenderThread() {
}

ChromeMockRenderThread::~ChromeMockRenderThread() {
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeMockRenderThread::GetIOMessageLoopProxy() {
  return io_task_runner_;
}

void ChromeMockRenderThread::set_io_message_loop_proxy(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  io_task_runner_ = task_runner;
}

bool ChromeMockRenderThread::OnMessageReceived(const IPC::Message& msg) {
  if (content::MockRenderThread::OnMessageReceived(msg))
    return true;

  // Some messages we do special handling.
#if defined(ENABLE_EXTENSIONS)
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeMockRenderThread, msg)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_OpenChannelToExtension,
                        OnOpenChannelToExtension)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
#else
  return false;
#endif
}

#if defined(ENABLE_EXTENSIONS)
void ChromeMockRenderThread::OnOpenChannelToExtension(
    int routing_id,
    const ExtensionMsg_ExternalConnectionInfo& info,
    const std::string& channel_name,
    bool include_tls_channel_id,
    int* port_id) {
  *port_id = 0;
}
#endif
