// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_EDGE_EFFECT_L_H_
#define CONTENT_BROWSER_ANDROID_EDGE_EFFECT_L_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/android/edge_effect_base.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class Layer;
class UIResourceLayer;
}

namespace ui {
class SystemUIResourceManager;
}

namespace content {

// |EdgeEffectL| mirrors its Android L counterpart, EdgeEffect.java.
// Conscious tradeoffs were made to align this as closely as possible with the
// the original Android java version.
// All coordinates and dimensions are in device pixels.
class EdgeEffectL : public EdgeEffectBase {
 public:
  explicit EdgeEffectL(ui::SystemUIResourceManager* resource_manager);
  virtual ~EdgeEffectL();

  virtual void Pull(base::TimeTicks current_time,
                    float delta_distance,
                    float displacement) OVERRIDE;
  virtual void Absorb(base::TimeTicks current_time, float velocity) OVERRIDE;
  virtual bool Update(base::TimeTicks current_time) OVERRIDE;
  virtual void Release(base::TimeTicks current_time) OVERRIDE;

  virtual void Finish() OVERRIDE;
  virtual bool IsFinished() const OVERRIDE;

  virtual void ApplyToLayers(const gfx::SizeF& size,
                             const gfx::Transform& transform) OVERRIDE;
  virtual void SetParent(cc::Layer* parent) OVERRIDE;

  // Thread-safe trigger to load resources.
  static void PreloadResources(ui::SystemUIResourceManager* resource_manager);

 private:
  ui::SystemUIResourceManager* const resource_manager_;

  scoped_refptr<cc::UIResourceLayer> glow_;

  float glow_alpha_;
  float glow_scale_y_;

  float glow_alpha_start_;
  float glow_alpha_finish_;
  float glow_scale_y_start_;
  float glow_scale_y_finish_;

  gfx::RectF arc_rect_;
  gfx::Size bounds_;
  float displacement_;
  float target_displacement_;

  base::TimeTicks start_time_;
  base::TimeDelta duration_;

  State state_;

  float pull_distance_;

  DISALLOW_COPY_AND_ASSIGN(EdgeEffectL);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_EDGE_EFFECT_L_H_
