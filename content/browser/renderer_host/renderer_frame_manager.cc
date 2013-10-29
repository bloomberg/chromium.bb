// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/renderer_frame_manager.h"

#include "base/logging.h"
#include "base/sys_info.h"

namespace content {

RendererFrameManager* RendererFrameManager::GetInstance() {
  return Singleton<RendererFrameManager>::get();
}

void RendererFrameManager::AddFrame(RendererFrameManagerClient* frame,
                                    bool visible) {
  RemoveFrame(frame);
  if (visible)
    visible_frames_.insert(frame);
  else
    hidden_frames_.push_front(frame);
  CullHiddenFrames();
}

void RendererFrameManager::RemoveFrame(RendererFrameManagerClient* frame) {
  visible_frames_.erase(frame);
  hidden_frames_.remove(frame);
}

void RendererFrameManager::SetFrameVisibility(RendererFrameManagerClient* frame,
                                              bool visible) {
  if (visible) {
    hidden_frames_.remove(frame);
    visible_frames_.insert(frame);
  } else {
    visible_frames_.erase(frame);
    hidden_frames_.push_front(frame);
    CullHiddenFrames();
  }
}

RendererFrameManager::RendererFrameManager()
    : max_number_of_saved_frames_(
          std::min(5, 2 + (base::SysInfo::AmountOfPhysicalMemoryMB() / 256))) {}

RendererFrameManager::~RendererFrameManager() {}

void RendererFrameManager::CullHiddenFrames() {
  while (!hidden_frames_.empty() &&
         hidden_frames_.size() + visible_frames_.size() >
             max_number_of_saved_frames()) {
    size_t old_size = hidden_frames_.size();
    // Should remove self from list.
    hidden_frames_.back()->EvictCurrentFrame();
    DCHECK_EQ(hidden_frames_.size() + 1, old_size);
  }
}

}  // namespace content
