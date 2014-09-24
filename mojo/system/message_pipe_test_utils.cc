// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/message_pipe_test_utils.h"

#include "base/bind.h"
#include "base/threading/platform_thread.h"  // For |Sleep()|.
#include "mojo/system/channel.h"
#include "mojo/system/channel_endpoint.h"
#include "mojo/system/message_pipe.h"
#include "mojo/system/waiter.h"

namespace mojo {
namespace system {
namespace test {

MojoResult WaitIfNecessary(scoped_refptr<MessagePipe> mp,
                           MojoHandleSignals signals,
                           HandleSignalsState* signals_state) {
  Waiter waiter;
  waiter.Init();

  MojoResult add_result = mp->AddWaiter(0, &waiter, signals, 0, signals_state);
  if (add_result != MOJO_RESULT_OK) {
    return (add_result == MOJO_RESULT_ALREADY_EXISTS) ? MOJO_RESULT_OK
                                                      : add_result;
  }

  MojoResult wait_result = waiter.Wait(MOJO_DEADLINE_INDEFINITE, nullptr);
  mp->RemoveWaiter(0, &waiter, signals_state);
  return wait_result;
}

ChannelThread::ChannelThread(embedder::PlatformSupport* platform_support)
    : platform_support_(platform_support),
      test_io_thread_(base::TestIOThread::kManualStart) {
}

ChannelThread::~ChannelThread() {
  Stop();
}

void ChannelThread::Start(embedder::ScopedPlatformHandle platform_handle,
                          scoped_refptr<ChannelEndpoint> channel_endpoint) {
  test_io_thread_.Start();
  test_io_thread_.PostTaskAndWait(
      FROM_HERE,
      base::Bind(&ChannelThread::InitChannelOnIOThread,
                 base::Unretained(this),
                 base::Passed(&platform_handle),
                 channel_endpoint));
}

void ChannelThread::Stop() {
  if (channel_.get()) {
    // Hack to flush write buffers before quitting.
    // TODO(vtl): Remove this once |Channel| has a
    // |FlushWriteBufferAndShutdown()| (or whatever).
    while (!channel_->IsWriteBufferEmpty())
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(20));

    test_io_thread_.PostTaskAndWait(
        FROM_HERE,
        base::Bind(&ChannelThread::ShutdownChannelOnIOThread,
                   base::Unretained(this)));
  }
  test_io_thread_.Stop();
}

void ChannelThread::InitChannelOnIOThread(
    embedder::ScopedPlatformHandle platform_handle,
    scoped_refptr<ChannelEndpoint> channel_endpoint) {
  CHECK_EQ(base::MessageLoop::current(), test_io_thread_.message_loop());
  CHECK(platform_handle.is_valid());

  // Create and initialize |Channel|.
  channel_ = new Channel(platform_support_);
  CHECK(channel_->Init(RawChannel::Create(platform_handle.Pass())));

  // Attach the message pipe endpoint.
  // Note: On the "server" (parent process) side, we need not attach the
  // message pipe endpoint immediately. However, on the "client" (child
  // process) side, this *must* be done here -- otherwise, the |Channel| may
  // receive/process messages (which it can do as soon as it's hooked up to
  // the IO thread message loop, and that message loop runs) before the
  // message pipe endpoint is attached.
  CHECK_EQ(channel_->AttachEndpoint(channel_endpoint),
           Channel::kBootstrapEndpointId);
  CHECK(channel_->RunMessagePipeEndpoint(Channel::kBootstrapEndpointId,
                                         Channel::kBootstrapEndpointId));
}

void ChannelThread::ShutdownChannelOnIOThread() {
  CHECK(channel_.get());
  channel_->Shutdown();
  channel_ = nullptr;
}

#if !defined(OS_IOS)
MultiprocessMessagePipeTestBase::MultiprocessMessagePipeTestBase()
    : channel_thread_(&platform_support_) {
}

MultiprocessMessagePipeTestBase::~MultiprocessMessagePipeTestBase() {
}

void MultiprocessMessagePipeTestBase::Init(scoped_refptr<ChannelEndpoint> ep) {
  channel_thread_.Start(helper_.server_platform_handle.Pass(), ep);
}
#endif

}  // namespace test
}  // namespace system
}  // namespace mojo
