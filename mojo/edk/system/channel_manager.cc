// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/channel_manager.h"

#include "base/bind.h"
#include "base/location.h"

namespace mojo {
namespace system {

namespace {

void ShutdownChannelHelper(const ChannelInfo& channel_info) {
  channel_info.channel->WillShutdownSoon();
  channel_info.channel_thread_task_runner->PostTask(
      FROM_HERE, base::Bind(&Channel::Shutdown, channel_info.channel));
}

}  // namespace

ChannelManager::ChannelManager() {
}

ChannelManager::~ChannelManager() {
  // No need to take the lock.
  for (const auto& map_elem : channel_infos_)
    ShutdownChannelHelper(map_elem.second);
}

ChannelId ChannelManager::AddChannel(
    scoped_refptr<Channel> channel,
    scoped_refptr<base::TaskRunner> channel_thread_task_runner) {
  ChannelId channel_id = GetChannelId(channel.get());

  {
    base::AutoLock locker(lock_);
    DCHECK(channel_infos_.find(channel_id) == channel_infos_.end());
    channel_infos_[channel_id] =
        ChannelInfo(channel, channel_thread_task_runner);
  }
  channel->SetChannelManager(this);

  return channel_id;
}

void ChannelManager::ShutdownChannel(ChannelId channel_id) {
  ChannelInfo channel_info;
  {
    base::AutoLock locker(lock_);

    auto it = channel_infos_.find(channel_id);
    DCHECK(it != channel_infos_.end());
    channel_info.Swap(&it->second);
    channel_infos_.erase(it);
  }
  ShutdownChannelHelper(channel_info);
}

}  // namespace system
}  // namespace mojo
