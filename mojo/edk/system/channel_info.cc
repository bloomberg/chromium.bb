// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/channel_info.h"

namespace mojo {
namespace system {

ChannelInfo::ChannelInfo() {
}

ChannelInfo::ChannelInfo(
    scoped_refptr<Channel> channel,
    scoped_refptr<base::TaskRunner> channel_thread_task_runner)
    : channel(channel), channel_thread_task_runner(channel_thread_task_runner) {
}

ChannelInfo::~ChannelInfo() {
}

}  // namespace system
}  // namespace mojo
