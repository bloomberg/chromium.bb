// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_FRAME_DETATCHED_TITLE_AREA_RENDERER_HOST_H_
#define ASH_MUS_FRAME_DETATCHED_TITLE_AREA_RENDERER_HOST_H_

namespace ash {
namespace mus {

class DetachedTitleAreaRenderer;

// Used to inform the creator of DetachedTitleAreaRenderer of interesting
// events.
class DetachedTitleAreaRendererHost {
 public:
  virtual void OnDetachedTitleAreaRendererDestroyed(
      DetachedTitleAreaRenderer* renderer) = 0;

 protected:
  virtual ~DetachedTitleAreaRendererHost() {}
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_FRAME_DETATCHED_TITLE_AREA_RENDERER_HOST_H_
