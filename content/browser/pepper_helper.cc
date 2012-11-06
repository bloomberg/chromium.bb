// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/pepper_helper.h"

#include "content/browser/renderer_host/pepper/pepper_message_filter.h"
#include "ipc/ipc_channel_proxy.h"
#include "net/base/host_resolver.h"

namespace content {

void EnablePepperSupportForChannel(IPC::ChannelProxy* channel,
                                   net::HostResolver* host_resolver,
                                   int process_id,
                                   int render_view_id) {
  channel->AddFilter(new PepperMessageFilter(PepperMessageFilter::NACL,
                                             host_resolver,
                                             process_id,
                                             render_view_id));
}

}  // namespace content

