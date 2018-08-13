// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/portal/portal.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/blink/public/common/features.h"

namespace content {

Portal::Portal(RenderFrameHostImpl* owner_render_frame_host)
    : WebContentsObserver(
          WebContents::FromRenderFrameHost(owner_render_frame_host)),
      owner_render_frame_host_(owner_render_frame_host) {}

Portal::~Portal() {}

// static
bool Portal::IsEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kPortals) ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableExperimentalWebPlatformFeatures);
}

// static
Portal* Portal::Create(RenderFrameHostImpl* owner_render_frame_host,
                       blink::mojom::PortalRequest request) {
  auto portal_ptr = base::WrapUnique(new Portal(owner_render_frame_host));
  Portal* portal = portal_ptr.get();
  mojo::StrongBinding<blink::mojom::Portal>::Create(std::move(portal_ptr),
                                                    std::move(request));
  return portal;
}

void Portal::RenderFrameDeleted(RenderFrameHost* render_frame_host) {
  if (render_frame_host == owner_render_frame_host_)
    owner_render_frame_host_ = nullptr;
}

}  // namespace content
