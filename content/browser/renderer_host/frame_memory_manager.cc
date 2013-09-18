// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/frame_memory_manager.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/sys_info.h"

namespace content {

namespace {

size_t MaxNumberOfSavedFrames() {
  return std::min(5, 2 + (base::SysInfo::AmountOfPhysicalMemoryMB() / 256));
}

}  // namespace

FrameMemoryManager* FrameMemoryManager::GetInstance() {
  return Singleton<FrameMemoryManager>::get();
}

void FrameMemoryManager::AddFrame(FrameContainer* frame, bool visible) {
  RemoveFrame(frame);
  if (visible)
    visible_frames_.insert(frame);
  else
    hidden_frames_.push_front(frame);
  CullHiddenFrames();
}

void FrameMemoryManager::RemoveFrame(FrameContainer* frame) {
  visible_frames_.erase(frame);
  hidden_frames_.remove(frame);
}

void FrameMemoryManager::SetFrameVisibility(FrameContainer* frame,
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

FrameMemoryManager::FrameMemoryManager() {}

FrameMemoryManager::~FrameMemoryManager() {}

void FrameMemoryManager::CullHiddenFrames() {
  while (!hidden_frames_.empty() &&
         hidden_frames_.size() + visible_frames_.size() >
             MaxNumberOfSavedFrames()) {
    size_t old_size = hidden_frames_.size();
    // Should remove self from list.
    hidden_frames_.back()->ReleaseCurrentFrame();
    DCHECK_EQ(hidden_frames_.size() + 1, old_size);
  }
}

}  // namespace content
