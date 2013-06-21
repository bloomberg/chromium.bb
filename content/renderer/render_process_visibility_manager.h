// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_PROCESS_VISIBILITY_MANAGER_H_
#define CONTENT_RENDERER_RENDER_PROCESS_VISIBILITY_MANAGER_H_

#include "base/memory/singleton.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT RenderProcessVisibilityManager {
 public:
  static RenderProcessVisibilityManager* GetInstance();

  void WidgetVisibilityChanged(bool visible);

 private:
  friend struct DefaultSingletonTraits<RenderProcessVisibilityManager>;

  RenderProcessVisibilityManager();
  ~RenderProcessVisibilityManager();

  int num_visible_render_widgets_;

  DISALLOW_COPY_AND_ASSIGN(RenderProcessVisibilityManager);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_PROCESS_VISIBILITY_MANAGER_H_
