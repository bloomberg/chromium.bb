// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_proxy_host.h"

#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/site_instance_impl.h"

namespace content {

RenderFrameProxyHost::RenderFrameProxyHost(
    scoped_ptr<RenderFrameHostImpl> render_frame_host)
    : site_instance_(render_frame_host->GetSiteInstance()),
      render_frame_host_(render_frame_host.Pass()) {}

RenderFrameProxyHost::~RenderFrameProxyHost() {}

}  // namespace content
