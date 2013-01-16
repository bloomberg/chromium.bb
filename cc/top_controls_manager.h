// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TOP_CONTROLS_MANAGER_H_
#define CC_TOP_CONTROLS_MANAGER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/layer_impl.h"
#include "ui/gfx/size.h"
#include "ui/gfx/vector2d_f.h"

namespace base {
class TimeTicks;
}

namespace cc {

class KeyframedFloatAnimationCurve;
class LayerTreeImpl;
class TopControlsManagerClient;

// Manages the position of the top controls.
class CC_EXPORT TopControlsManager {
 public:
  static scoped_ptr<TopControlsManager> Create(TopControlsManagerClient* client,
                                               float top_controls_height);
  virtual ~TopControlsManager();

  void set_content_layer_id(int id) { content_layer_id_ = id; }
  float controls_top_offset() { return controls_top_offset_; }
  float content_top_offset() { return content_top_offset_; }
  float is_overlay_mode() { return is_overlay_mode_; }
  KeyframedFloatAnimationCurve* animation() {
    return top_controls_animation_.get();
  }

  void UpdateDrawPositions();

  void ScrollBegin();
  gfx::Vector2dF ScrollBy(const gfx::Vector2dF pending_delta);
  void ScrollEnd();

  void Animate(base::TimeTicks monotonic_time);

 protected:
  TopControlsManager(TopControlsManagerClient* client,
                     float top_controls_height);

 private:
  gfx::Vector2dF ScrollInternal(const gfx::Vector2dF pending_delta);
  void ResetAnimations();
  LayerImpl* RootScrollLayer();
  float RootScrollLayerTotalScrollY();
  void SetupAnimation(bool show_controls);
  void StartAnimationIfNecessary();

  TopControlsManagerClient* client_;  // The client manages the lifecycle of
                                      // this.

  scoped_ptr<KeyframedFloatAnimationCurve> top_controls_animation_;
  int content_layer_id_;
  bool is_showing_animation_;
  bool is_overlay_mode_;
  float controls_top_offset_;
  float content_top_offset_;
  float top_controls_height_;
  bool scroll_readjustment_enabled_;
  float previous_root_scroll_offset_;

  DISALLOW_COPY_AND_ASSIGN(TopControlsManager);
};

}  // namespace cc

#endif  // CC_TOP_CONTROLS_MANAGER_H_
