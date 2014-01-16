// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/embedder.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/system/channel.h"
#include "mojo/system/core_impl.h"
#include "mojo/system/local_message_pipe_endpoint.h"
#include "mojo/system/message_pipe.h"
#include "mojo/system/message_pipe_dispatcher.h"
#include "mojo/system/proxy_message_pipe_endpoint.h"

namespace mojo {

namespace embedder {

struct ChannelInfo {
  scoped_refptr<system::Channel> channel;
};

}  // namespace embedder

// Have helpers in the |system| namespace, to avoid saying "system::" all over
// the place.
namespace system {
namespace {

void CreateChannelOnIOThread(
    ScopedPlatformHandle platform_handle,
    scoped_refptr<MessagePipe> message_pipe,
    embedder::DidCreateChannelOnIOThreadCallback callback) {
  CHECK(platform_handle.is_valid());

  scoped_ptr<embedder::ChannelInfo> channel_info(new embedder::ChannelInfo);

  // Create and initialize |Channel|.
  channel_info->channel = new Channel();
  bool success = channel_info->channel->Init(platform_handle.Pass());
  DCHECK(success);

  // Attach the message pipe endpoint.
  MessageInTransit::EndpointId endpoint_id =
      channel_info->channel->AttachMessagePipeEndpoint(message_pipe, 1);
  DCHECK_EQ(endpoint_id, Channel::kBootstrapEndpointId);
  channel_info->channel->RunMessagePipeEndpoint(Channel::kBootstrapEndpointId,
                                                Channel::kBootstrapEndpointId);

  // Hand the channel back to the embedder.
  callback.Run(channel_info.release());
}

MojoHandle CreateChannelHelper(
    ScopedPlatformHandle platform_handle,
    scoped_refptr<base::TaskRunner> io_thread_task_runner,
    embedder::DidCreateChannelOnIOThreadCallback callback) {
  DCHECK(platform_handle.is_valid());

  scoped_refptr<MessagePipe> message_pipe(new MessagePipe(
      scoped_ptr<MessagePipeEndpoint>(new LocalMessagePipeEndpoint()),
      scoped_ptr<MessagePipeEndpoint>(new ProxyMessagePipeEndpoint())));
  scoped_refptr<MessagePipeDispatcher> dispatcher(new MessagePipeDispatcher());
  dispatcher->Init(message_pipe, 0);

  CoreImpl* core_impl = static_cast<CoreImpl*>(Core::Get());
  DCHECK(core_impl);
  MojoHandle rv = core_impl->AddDispatcher(dispatcher);
  // TODO(vtl): Do we properly handle the failure case here?
  if (rv != MOJO_HANDLE_INVALID) {
    io_thread_task_runner->PostTask(FROM_HERE,
                                    base::Bind(&CreateChannelOnIOThread,
                                               base::Passed(&platform_handle),
                                               message_pipe,
                                               callback));
  }
  return rv;
}

}  // namespace
}  // namespace system

namespace embedder {

void Init() {
  Core::Init(new system::CoreImpl());
}

MojoHandle CreateChannel(
    system::ScopedPlatformHandle platform_handle,
    scoped_refptr<base::TaskRunner> io_thread_task_runner,
    DidCreateChannelOnIOThreadCallback callback) {
  return system::CreateChannelHelper(platform_handle.Pass(),
                                     io_thread_task_runner,
                                     callback);
}

void DestroyChannelOnIOThread(ChannelInfo* channel_info) {
  DCHECK(channel_info);
  DCHECK(channel_info->channel.get());
  channel_info->channel->Shutdown();
  delete channel_info;
}

}  // namespace embedder
}  // namespace mojo
