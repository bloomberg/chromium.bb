// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/embedder.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/embedder/platform_support.h"
#include "mojo/edk/system/channel.h"
#include "mojo/edk/system/channel_endpoint.h"
#include "mojo/edk/system/channel_manager.h"
#include "mojo/edk/system/configuration.h"
#include "mojo/edk/system/core.h"
#include "mojo/edk/system/message_pipe_dispatcher.h"
#include "mojo/edk/system/platform_handle_dispatcher.h"
#include "mojo/edk/system/raw_channel.h"

namespace mojo {
namespace embedder {

namespace {

// Helper for |CreateChannel...()|. Returns 0 on failure. Called on the channel
// creation thread.
system::ChannelId MakeChannel(
    ScopedPlatformHandle platform_handle,
    scoped_refptr<system::ChannelEndpoint> channel_endpoint) {
  DCHECK(platform_handle.is_valid());

  // Create and initialize a |system::Channel|.
  DCHECK(internal::g_core);
  scoped_refptr<system::Channel> channel =
      new system::Channel(internal::g_core->platform_support());
  if (!channel->Init(system::RawChannel::Create(platform_handle.Pass()))) {
    // This is very unusual (e.g., maybe |platform_handle| was invalid or we
    // reached some system resource limit).
    LOG(ERROR) << "Channel::Init() failed";
    // Return null, since |Shutdown()| shouldn't be called in this case.
    return 0;
  }

  channel->SetBootstrapEndpoint(channel_endpoint);

  DCHECK(internal::g_channel_manager);
  return internal::g_channel_manager->AddChannel(
      channel, base::MessageLoopProxy::current());
}

// Helper for |CreateChannel()|. Called on the channel creation thread.
void CreateChannelHelper(
    ScopedPlatformHandle platform_handle,
    scoped_ptr<ChannelInfo> channel_info,
    scoped_refptr<system::ChannelEndpoint> channel_endpoint,
    DidCreateChannelCallback callback,
    scoped_refptr<base::TaskRunner> callback_thread_task_runner) {
  channel_info->channel_id =
      MakeChannel(platform_handle.Pass(), channel_endpoint);

  // Hand the channel back to the embedder.
  if (callback_thread_task_runner) {
    callback_thread_task_runner->PostTask(
        FROM_HERE, base::Bind(callback, channel_info.release()));
  } else {
    callback.Run(channel_info.release());
  }
}

}  // namespace

namespace internal {

// Declared in embedder_internal.h.
system::Core* g_core = nullptr;
system::ChannelManager* g_channel_manager = nullptr;

}  // namespace internal

void Init(scoped_ptr<PlatformSupport> platform_support) {
  DCHECK(!internal::g_core);
  internal::g_core = new system::Core(platform_support.Pass());
  DCHECK(!internal::g_channel_manager);
  internal::g_channel_manager = new system::ChannelManager();
}

Configuration* GetConfiguration() {
  return system::GetMutableConfiguration();
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

  DCHECK(internal::g_core);
  ScopedMessagePipeHandle rv(
      MessagePipeHandle(internal::g_core->AddDispatcher(dispatcher)));

  *channel_info =
      new ChannelInfo(MakeChannel(platform_handle.Pass(), channel_endpoint));

  return rv.Pass();
}

ScopedMessagePipeHandle CreateChannel(
    ScopedPlatformHandle platform_handle,
    scoped_refptr<base::TaskRunner> io_thread_task_runner,
    DidCreateChannelCallback callback,
    scoped_refptr<base::TaskRunner> callback_thread_task_runner) {
  DCHECK(platform_handle.is_valid());
  DCHECK(io_thread_task_runner);
  DCHECK(!callback.is_null());

  scoped_refptr<system::ChannelEndpoint> channel_endpoint;
  scoped_refptr<system::MessagePipeDispatcher> dispatcher =
      system::MessagePipeDispatcher::CreateRemoteMessagePipe(&channel_endpoint);

  DCHECK(internal::g_core);
  ScopedMessagePipeHandle rv(
      MessagePipeHandle(internal::g_core->AddDispatcher(dispatcher)));

  // We'll have to set |channel_info->channel_id| on the I/O thread.
  scoped_ptr<ChannelInfo> channel_info(new ChannelInfo());

  if (rv.is_valid()) {
    io_thread_task_runner->PostTask(
        FROM_HERE,
        base::Bind(&CreateChannelHelper, base::Passed(&platform_handle),
                   base::Passed(&channel_info), channel_endpoint, callback,
                   callback_thread_task_runner));
  } else {
    (callback_thread_task_runner.get() ? callback_thread_task_runner
                                       : io_thread_task_runner)
        ->PostTask(FROM_HERE, base::Bind(callback, channel_info.release()));
  }

  return rv.Pass();
}

// TODO(vtl): Write tests for this.
void DestroyChannel(ChannelInfo* channel_info) {
  DCHECK(channel_info);
  if (!channel_info->channel_id) {
    // Presumably, |Init()| on the channel failed.
    return;
  }

  DCHECK(internal::g_channel_manager);
  // This will destroy the channel synchronously if called from the channel
  // thread.
  internal::g_channel_manager->ShutdownChannel(channel_info->channel_id);
  delete channel_info;
}

void WillDestroyChannelSoon(ChannelInfo* channel_info) {
  DCHECK(channel_info);
  DCHECK(internal::g_channel_manager);
  internal::g_channel_manager->WillShutdownChannel(channel_info->channel_id);
}

MojoResult CreatePlatformHandleWrapper(
    ScopedPlatformHandle platform_handle,
    MojoHandle* platform_handle_wrapper_handle) {
  DCHECK(platform_handle_wrapper_handle);

  scoped_refptr<system::Dispatcher> dispatcher(
      new system::PlatformHandleDispatcher(platform_handle.Pass()));

  DCHECK(internal::g_core);
  MojoHandle h = internal::g_core->AddDispatcher(dispatcher);
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

  DCHECK(internal::g_core);
  scoped_refptr<system::Dispatcher> dispatcher(
      internal::g_core->GetDispatcher(platform_handle_wrapper_handle));
  if (!dispatcher)
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
