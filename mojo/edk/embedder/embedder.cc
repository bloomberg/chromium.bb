// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/embedder.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "mojo/edk/embedder/platform_support.h"
#include "mojo/edk/system/channel.h"
#include "mojo/edk/system/channel_endpoint.h"
#include "mojo/edk/system/channel_info.h"
#include "mojo/edk/system/core.h"
#include "mojo/edk/system/entrypoints.h"
#include "mojo/edk/system/message_pipe_dispatcher.h"
#include "mojo/edk/system/platform_handle_dispatcher.h"
#include "mojo/edk/system/raw_channel.h"

namespace mojo {
namespace embedder {

namespace {

// Helper for |CreateChannel...()|. (Note: May return null for some failures.)
scoped_refptr<system::Channel> MakeChannel(
    system::Core* core,
    ScopedPlatformHandle platform_handle,
    scoped_refptr<system::ChannelEndpoint> channel_endpoint) {
  DCHECK(platform_handle.is_valid());

  // Create and initialize a |system::Channel|.
  scoped_refptr<system::Channel> channel =
      new system::Channel(core->platform_support());
  if (!channel->Init(system::RawChannel::Create(platform_handle.Pass()))) {
    // This is very unusual (e.g., maybe |platform_handle| was invalid or we
    // reached some system resource limit).
    LOG(ERROR) << "Channel::Init() failed";
    // Return null, since |Shutdown()| shouldn't be called in this case.
    return scoped_refptr<system::Channel>();
  }
  // Once |Init()| has succeeded, we have to return |channel| (since
  // |Shutdown()| will have to be called on it).

  channel->AttachAndRunEndpoint(channel_endpoint, true);
  return channel;
}

void CreateChannelHelper(
    system::Core* core,
    ScopedPlatformHandle platform_handle,
    scoped_ptr<ChannelInfo> channel_info,
    scoped_refptr<system::ChannelEndpoint> channel_endpoint,
    DidCreateChannelCallback callback,
    scoped_refptr<base::TaskRunner> callback_thread_task_runner) {
  channel_info->channel =
      MakeChannel(core, platform_handle.Pass(), channel_endpoint);

  // Hand the channel back to the embedder.
  if (callback_thread_task_runner.get()) {
    callback_thread_task_runner->PostTask(
        FROM_HERE, base::Bind(callback, channel_info.release()));
  } else {
    callback.Run(channel_info.release());
  }
}

}  // namespace

void Init(scoped_ptr<PlatformSupport> platform_support) {
  system::entrypoints::SetCore(new system::Core(platform_support.Pass()));
}

// TODO(vtl): Write tests for this.
ScopedMessagePipeHandle CreateChannelOnIOThread(
    ScopedPlatformHandle platform_handle,
    ChannelInfo** channel_info) {
  DCHECK(platform_handle.is_valid());
  DCHECK(channel_info);

  scoped_refptr<system::ChannelEndpoint> channel_endpoint;
  scoped_refptr<system::MessagePipeDispatcher> dispatcher =
      system::MessagePipeDispatcher::CreateRemoteMessagePipe(&channel_endpoint);

  system::Core* core = system::entrypoints::GetCore();
  DCHECK(core);
  ScopedMessagePipeHandle rv(
      MessagePipeHandle(core->AddDispatcher(dispatcher)));

  *channel_info = new ChannelInfo(
      MakeChannel(core, platform_handle.Pass(), channel_endpoint),
      base::MessageLoopProxy::current());

  return rv.Pass();
}

ScopedMessagePipeHandle CreateChannel(
    ScopedPlatformHandle platform_handle,
    scoped_refptr<base::TaskRunner> io_thread_task_runner,
    DidCreateChannelCallback callback,
    scoped_refptr<base::TaskRunner> callback_thread_task_runner) {
  DCHECK(platform_handle.is_valid());
  DCHECK(io_thread_task_runner.get());
  DCHECK(!callback.is_null());

  scoped_refptr<system::ChannelEndpoint> channel_endpoint;
  scoped_refptr<system::MessagePipeDispatcher> dispatcher =
      system::MessagePipeDispatcher::CreateRemoteMessagePipe(&channel_endpoint);

  system::Core* core = system::entrypoints::GetCore();
  DCHECK(core);
  ScopedMessagePipeHandle rv(
      MessagePipeHandle(core->AddDispatcher(dispatcher)));

  scoped_ptr<ChannelInfo> channel_info(new ChannelInfo());
  // We'll have to set |channel_info->channel| on the I/O thread.
  channel_info->channel_thread_task_runner = io_thread_task_runner;

  if (rv.is_valid()) {
    io_thread_task_runner->PostTask(FROM_HERE,
                                    base::Bind(&CreateChannelHelper,
                                               base::Unretained(core),
                                               base::Passed(&platform_handle),
                                               base::Passed(&channel_info),
                                               channel_endpoint,
                                               callback,
                                               callback_thread_task_runner));
  } else {
    (callback_thread_task_runner.get() ? callback_thread_task_runner
                                       : io_thread_task_runner)
        ->PostTask(FROM_HERE, base::Bind(callback, channel_info.release()));
  }

  return rv.Pass();
}

void DestroyChannelOnIOThread(ChannelInfo* channel_info) {
  DCHECK(channel_info);
  if (!channel_info->channel.get()) {
    // Presumably, |Init()| on the channel failed.
    return;
  }

  channel_info->channel->Shutdown();
  delete channel_info;
}

// TODO(vtl): Write tests for this.
void DestroyChannel(ChannelInfo* channel_info) {
  DCHECK(channel_info);
  DCHECK(channel_info->channel_thread_task_runner.get());

  if (!channel_info->channel.get()) {
    // Presumably, |Init()| on the channel failed.
    return;
  }

  channel_info->channel->WillShutdownSoon();
  channel_info->channel_thread_task_runner->PostTask(
      FROM_HERE, base::Bind(&DestroyChannelOnIOThread, channel_info));
}

void WillDestroyChannelSoon(ChannelInfo* channel_info) {
  DCHECK(channel_info);
  channel_info->channel->WillShutdownSoon();
}

MojoResult CreatePlatformHandleWrapper(
    ScopedPlatformHandle platform_handle,
    MojoHandle* platform_handle_wrapper_handle) {
  DCHECK(platform_handle_wrapper_handle);

  scoped_refptr<system::Dispatcher> dispatcher(
      new system::PlatformHandleDispatcher(platform_handle.Pass()));

  system::Core* core = system::entrypoints::GetCore();
  DCHECK(core);
  MojoHandle h = core->AddDispatcher(dispatcher);
  if (h == MOJO_HANDLE_INVALID) {
    LOG(ERROR) << "Handle table full";
    dispatcher->Close();
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  }

  *platform_handle_wrapper_handle = h;
  return MOJO_RESULT_OK;
}

MojoResult PassWrappedPlatformHandle(MojoHandle platform_handle_wrapper_handle,
                                     ScopedPlatformHandle* platform_handle) {
  DCHECK(platform_handle);

  system::Core* core = system::entrypoints::GetCore();
  DCHECK(core);
  scoped_refptr<system::Dispatcher> dispatcher(
      core->GetDispatcher(platform_handle_wrapper_handle));
  if (!dispatcher.get())
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (dispatcher->GetType() != system::Dispatcher::kTypePlatformHandle)
    return MOJO_RESULT_INVALID_ARGUMENT;

  *platform_handle =
      static_cast<system::PlatformHandleDispatcher*>(dispatcher.get())
          ->PassPlatformHandle()
          .Pass();
  return MOJO_RESULT_OK;
}

}  // namespace embedder
}  // namespace mojo
