// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/delegated_frame_evictor.h"

namespace content {

DelegatedFrameEvictor::DelegatedFrameEvictor(
    DelegatedFrameEvictorClient* client)
    : client_(client), has_frame_(false) {}

DelegatedFrameEvictor::~DelegatedFrameEvictor() { DiscardedFrame(); }

void DelegatedFrameEvictor::SwappedFrame(bool visible) {
  has_frame_ = true;
  RendererFrameManager::GetInstance()->AddFrame(this, visible);
}

void DelegatedFrameEvictor::DiscardedFrame() {
  RendererFrameManager::GetInstance()->RemoveFrame(this);
  has_frame_ = false;
}

void DelegatedFrameEvictor::SetVisible(bool visible) {
  if (has_frame_)
    RendererFrameManager::GetInstance()->SetFrameVisibility(this, visible);
}

void DelegatedFrameEvictor::EvictCurrentFrame() {
  client_->EvictDelegatedFrame();
}

}  // namespace content
