// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/embedder/embedder.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/system/channel.h"
#include "mojo/system/core_impl.h"
#include "mojo/system/message_pipe.h"
#include "mojo/system/message_pipe_dispatcher.h"
#include "mojo/system/raw_channel.h"

namespace mojo {
namespace embedder {

struct ChannelInfo {
  scoped_refptr<system::Channel> channel;
};

static void CreateChannelOnIOThread(
    ScopedPlatformHandle platform_handle,
    scoped_refptr<system::MessagePipe> message_pipe,
    DidCreateChannelCallback callback,
    scoped_refptr<base::TaskRunner> callback_thread_task_runner) {
  CHECK(platform_handle.is_valid());

  scoped_ptr<ChannelInfo> channel_info(new ChannelInfo);

  // Create and initialize a |system::Channel|.
  channel_info->channel = new system::Channel();
  bool success = channel_info->channel->Init(
      system::RawChannel::Create(platform_handle.Pass()));
  DCHECK(success);

  // Attach the message pipe endpoint.
  system::MessageInTransit::EndpointId endpoint_id =
      channel_info->channel->AttachMessagePipeEndpoint(message_pipe, 1);
  DCHECK_EQ(endpoint_id, system::Channel::kBootstrapEndpointId);
  channel_info->channel->RunMessagePipeEndpoint(
      system::Channel::kBootstrapEndpointId,
      system::Channel::kBootstrapEndpointId);

  // Hand the channel back to the embedder.
  if (callback_thread_task_runner) {
    callback_thread_task_runner->PostTask(FROM_HERE,
                                          base::Bind(callback,
                                                     channel_info.release()));
  } else {
    callback.Run(channel_info.release());
  }
}

void Init() {
  Core::Init(new system::CoreImpl());
}

ScopedMessagePipeHandle CreateChannel(
    ScopedPlatformHandle platform_handle,
    scoped_refptr<base::TaskRunner> io_thread_task_runner,
    DidCreateChannelCallback callback,
    scoped_refptr<base::TaskRunner> callback_thread_task_runner) {
  DCHECK(platform_handle.is_valid());

  std::pair<scoped_refptr<system::MessagePipeDispatcher>,
            scoped_refptr<system::MessagePipe> > remote_message_pipe =
      system::MessagePipeDispatcher::CreateRemoteMessagePipe();

  system::CoreImpl* core_impl = static_cast<system::CoreImpl*>(Core::Get());
  DCHECK(core_impl);
  ScopedMessagePipeHandle rv(
      MessagePipeHandle(core_impl->AddDispatcher(remote_message_pipe.first)));
  // TODO(vtl): Do we properly handle the failure case here?
  if (rv.is_valid()) {
    io_thread_task_runner->PostTask(FROM_HERE,
                                    base::Bind(&CreateChannelOnIOThread,
                                               base::Passed(&platform_handle),
                                               remote_message_pipe.second,
                                               callback,
                                               callback_thread_task_runner));
  }
  return rv.Pass();
}

void DestroyChannelOnIOThread(ChannelInfo* channel_info) {
  DCHECK(channel_info);
  DCHECK(channel_info->channel.get());
  channel_info->channel->Shutdown();
  delete channel_info;
}

}  // namespace embedder
}  // namespace mojo
