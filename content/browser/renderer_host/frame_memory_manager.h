// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_FRAME_MEMORY_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_FRAME_MEMORY_MANAGER_H_

#include <list>
#include <set>

#include "base/basictypes.h"

template <typename T> struct DefaultSingletonTraits;

namespace content {

class FrameContainer {
 public:
  virtual void ReleaseCurrentFrame() = 0;
};

class FrameMemoryManager {
 public:
  static FrameMemoryManager* GetInstance();

  void AddFrame(FrameContainer*, bool visible);
  void RemoveFrame(FrameContainer*);
  void SetFrameVisibility(FrameContainer*, bool visible);

 private:
  FrameMemoryManager();
  ~FrameMemoryManager();
  void CullHiddenFrames();
  friend struct DefaultSingletonTraits<FrameMemoryManager>;

  std::set<FrameContainer*> visible_frames_;
  std::list<FrameContainer*> hidden_frames_;

  DISALLOW_COPY_AND_ASSIGN(FrameMemoryManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_FRAME_MEMORY_MANAGER_H_
