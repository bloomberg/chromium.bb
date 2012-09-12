// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PEPPER_HELPER_H_
#define CONTENT_PUBLIC_BROWSER_PEPPER_HELPER_H_

#include "content/common/content_export.h"

namespace IPC {
class ChannelProxy;
}
namespace net {
class HostResolver;
}

namespace content {

// Enables dispatching PPAPI messages from the plugin to the browser.
CONTENT_EXPORT void EnablePepperSupportForChannel(
    IPC::ChannelProxy* channel,
    net::HostResolver* host_resolver);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PEPPER_HELPER_H_

