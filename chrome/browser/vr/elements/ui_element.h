// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_H_
#define CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "cc/animation/animation_target.h"
#include "cc/animation/transform_operations.h"
#include "chrome/browser/vr/animation_player.h"
#include "chrome/browser/vr/color_scheme.h"
#include "chrome/browser/vr/elements/ui_element_debug_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/transform.h"

namespace base {
class TimeTicks;
}

namespace blink {
class WebGestureEvent;
}

namespace vr {

class Animation;
class UiElementRenderer;
class UiElementTransformOperations;

enum XAnchoring {
  XNONE = 0,
  XLEFT,
  XRIGHT,
};

enum YAnchoring {
  YNONE = 0,
  YTOP,
  YBOTTOM,
};

class UiElement : public cc::AnimationTarget {
 public:
  UiElement();
  ~UiElement() override;

  enum OperationIndex {
    kTranslateIndex = 0,
    kRotateIndex = 1,
    kScaleIndex = 2,
  };

  virtual void PrepareToDraw();

  void Animate(const base::TimeTicks& time);

  // Indicates whether the element should be visually rendered.
  bool IsVisible() const;

  // Indicates whether the element should be tested for cursor input.
  bool IsHitTestable() const;

  virtual void Render(UiElementRenderer* renderer,
                      const gfx::Transform& view_proj_matrix) const;

  virtual void Initialize();

  // Controller interaction methods.
  virtual void OnHoverEnter(const gfx::PointF& position);
  virtual void OnHoverLeave();
  virtual void OnMove(const gfx::PointF& position);
  virtual void OnButtonDown(const gfx::PointF& position);
  virtual void OnButtonUp(const gfx::PointF& position);
  virtual void OnFlingStart(std::unique_ptr<blink::WebGestureEvent> gesture,
                            const gfx::PointF& position);
  virtual void OnFlingCancel(std::unique_ptr<blink::WebGestureEvent> gesture,
                             const gfx::PointF& position);
  virtual void OnScrollBegin(std::unique_ptr<blink::WebGestureEvent> gesture,
                             const gfx::PointF& position);
  virtual void OnScrollUpdate(std::unique_ptr<blink::WebGestureEvent> gesture,
                              const gfx::PointF& position);
  virtual void OnScrollEnd(std::unique_ptr<blink::WebGestureEvent> gesture,
                           const gfx::PointF& position);

  // Whether the point (relative to the origin of the element), should be
  // considered on the element. All elements are considered rectangular by
  // default though elements may override this function to handle arbitrary
  // shapes. Points within the rectangular area are mapped from 0:1 as follows,
  // though will extend outside this range when outside of the element:
  // [(0.0, 0.0), (1.0, 0.0)
  //  (1.0, 0.0), (1.0, 1.0)]
  virtual bool HitTest(const gfx::PointF& point) const;

  int id() const { return id_; }
  void set_id(int id) { id_ = id; }

  // If true, this object will be visible.
  bool visible() const { return visible_; }
  void SetVisible(bool visible);

  // If false, the reticle will not hit the element, even if visible.
  bool hit_testable() const { return hit_testable_; }
  void set_hit_testable(bool hit_testable) { hit_testable_ = hit_testable; }

  // TODO(bshe): We might be able to remove this state.
  bool viewport_aware() const { return viewport_aware_; }
  void set_viewport_aware(bool enable) { viewport_aware_ = enable; }
  bool computed_viewport_aware() const { return computed_viewport_aware_; }
  void set_computed_viewport_aware(bool computed_lock) {
    computed_viewport_aware_ = computed_lock;
  }

  // If true should be drawn in the world viewport, but over all other elements.
  bool is_overlay() const { return is_overlay_; }
  void set_is_overlay(bool is_overlay) { is_overlay_ = is_overlay; }

  bool scrollable() const { return scrollable_; }
  void set_scrollable(bool scrollable) { scrollable_ = scrollable; }

  // The size of the object.  This does not affect children.
  gfx::SizeF size() const { return size_; }
  void SetSize(float width, float hight);

  // It is assumed that operations is of size 4 with a component for layout
  // translation, translation, rotation and scale, in that order (see
  // constructor and the DCHECKs in the implementation of this function).
  void SetTransformOperations(const UiElementTransformOperations& operations);

  // These are convenience functions for setting the transform operations. They
  // will animate if you've set a transition. If you need to animate more than
  // one operation simultaneously, please use |SetTransformOperations| below.
  void SetLayoutOffset(float x, float y);
  void SetTranslate(float x, float y, float z);
  void SetRotate(float x, float y, float z, float radians);
  void SetScale(float x, float y, float z);

  AnimationPlayer& animation_player() { return animation_player_; }

  // The opacity of the object (between 0.0 and 1.0).
  float opacity() const { return opacity_; }
  void SetOpacity(float opacity);

  // The corner radius of the object. Analogous to CSS's border-radius. This is
  // in meters (same units as |size|).
  float corner_radius() const { return corner_radius_; }
  void set_corner_radius(float corner_radius) {
    corner_radius_ = corner_radius;
  }

  // The computed opacity, incorporating opacity of parent objects.
  float computed_opacity() const { return computed_opacity_; }
  void set_computed_opacity(float computed_opacity) {
    computed_opacity_ = computed_opacity;
  }

  // If anchoring is specified, the translation will be relative to the
  // specified edge(s) of the parent, rather than the center.  A parent object
  // must be specified when using anchoring.
  XAnchoring x_anchoring() const { return x_anchoring_; }
  void set_x_anchoring(XAnchoring x_anchoring) { x_anchoring_ = x_anchoring; }

  YAnchoring y_anchoring() const { return y_anchoring_; }
  void set_y_anchoring(YAnchoring y_anchoring) { y_anchoring_ = y_anchoring; }

  int draw_phase() const { return draw_phase_; }
  void set_draw_phase(int draw_phase) { draw_phase_ = draw_phase; }

  // This transform can be used by children to derive position of its parent.
  const gfx::Transform& inheritable_transform() const {
    return inheritable_transform_;
  }
  void set_inheritable_transform(const gfx::Transform& transform) {
    inheritable_transform_ = transform;
  }

  // A flag usable during transformation calculates to avoid duplicate work.
  bool dirty() const { return dirty_; }
  void set_dirty(bool dirty) { dirty_ = dirty; }

  // A flag usable during transformation calculates to avoid duplicate work.
  UiElementDebugId debug_id() const { return debug_id_; }
  void set_debug_id(UiElementDebugId debug_id) { debug_id_ = debug_id; }

  // By default, sets an element to be visible or not. This may be overridden to
  // allow finer control of element visibility.
  virtual void SetEnabled(bool enabled);

  void SetMode(ColorScheme::Mode mode);
  ColorScheme::Mode mode() const { return mode_; }

  const gfx::Transform& world_space_transform() const {
    return world_space_transform_;
  }
  void set_world_space_transform(const gfx::Transform& transform) {
    world_space_transform_ = transform;
  }

  // Transformations are applied relative to the parent element, rather than
  // absolutely. You cannot currently unparent elements.
  // TODO(vollick): elements should own their children. UiScene can turn into
  // recursive operations on the UiElement tree.
  void AddChild(UiElement* child);
  UiElement* parent() { return parent_; }

  gfx::Point3F GetCenter() const;
  gfx::Vector3dF GetNormal() const;

  // Computes the distance from |ray_origin| to this rectangles's plane, along
  // |ray_vector|. Returns true and populates |distance| if the calculation is
  // possible, and false if the ray is parallel to the plane.
  bool GetRayDistance(const gfx::Point3F& ray_origin,
                      const gfx::Vector3dF& ray_vector,
                      float* distance) const;

  // Projects a 3D world point onto the X and Y axes of the transformed
  // rectangle, returning 2D coordinates relative to the un-transformed unit
  // rectangle. This allows beam intersection points to be mapped to sprite
  // pixel coordinates. Points that fall onto the rectangle will generate X and
  // Y values on the interval [-0.5, 0.5].
  gfx::PointF GetUnitRectangleCoordinates(
      const gfx::Point3F& world_point) const;

  // cc::AnimationTarget
  void NotifyClientFloatAnimated(float opacity,
                                 int transform_property_id,
                                 cc::Animation* animation) override;
  void NotifyClientTransformOperationsAnimated(
      const cc::TransformOperations& operations,
      int transform_property_id,
      cc::Animation* animation) override;
  void NotifyClientSizeAnimated(const gfx::SizeF& size,
                                int transform_property_id,
                                cc::Animation* animation) override;
  void NotifyClientBooleanAnimated(bool visible,
                                   int transform_property_id,
                                   cc::Animation* animation) override;

  // Handles positioning adjustments for children. This will be overridden by
  // UiElements providing custom layout modes. See the documentation of the
  // override for their particular functionality. The base implementation
  // applies anchoring.
  virtual void LayOutChildren();

  virtual gfx::Transform LocalTransform() const;

  // Handles positioning adjustment for element which may reposition itself
  // automatically under certain circumstances. For example, viewport aware
  // element needs to reposition itself when the element is too far to the left
  // or right where the head is pointing.
  virtual void AdjustRotationForHeadPose(const gfx::Vector3dF& look_at);

 protected:
  virtual void OnSetMode();

  std::vector<UiElement*>& children() { return children_; }

  base::TimeTicks last_frame_time() const { return last_frame_time_; }

 private:
  // Valid IDs are non-negative.
  int id_ = -1;

  // If true, this object will be visible.
  bool visible_ = false;

  // If false, the reticle will not hit the element, even if visible.
  bool hit_testable_ = true;

  // If true, the element will reposition itself to viewport if neccessary.
  bool viewport_aware_ = false;

  // The computed viewport aware, incorporating from parent objects.
  bool computed_viewport_aware_ = false;

  // If true, then this element will be drawn in the world viewport, but above
  // all other elements.
  bool is_overlay_ = false;

  bool scrollable_ = false;

  // The size of the object.  This does not affect children.
  gfx::SizeF size_ = {1.0f, 1.0f};

  // The opacity of the object (between 0.0 and 1.0).
  float opacity_ = 1.0f;

  float corner_radius_ = 0.0f;

  // The computed opacity, incorporating opacity of parent objects.
  float computed_opacity_ = 1.0f;

  // If anchoring is specified, the translation will be relative to the
  // specified edge(s) of the parent, rather than the center.  A parent object
  // must be specified when using anchoring.
  XAnchoring x_anchoring_ = XAnchoring::XNONE;
  YAnchoring y_anchoring_ = YAnchoring::YNONE;

  AnimationPlayer animation_player_;

  int draw_phase_ = -1;

  // This is the time as of the last call to |Animate|. It is needed when
  // reversing transitions.
  base::TimeTicks last_frame_time_;

  // This transform can be used by children to derive position of its parent.
  gfx::Transform inheritable_transform_;

  // A flag usable during transformation calculates to avoid duplicate work.
  bool dirty_ = false;

  // An identifier used for testing and debugging, in lieu of a string.
  UiElementDebugId debug_id_ = UiElementDebugId::kNone;

  // This local transform operations. They are inherited by descendants and are
  // stored as a list of operations rather than a baked transform to make
  // transitions easier to implement (you may, for example, want to animate just
  // the translation, but leave the rotation and scale in tact).
  cc::TransformOperations transform_operations_;

  // This is set by the parent and is combined into LocalTransform()
  cc::TransformOperations layout_offset_;

  // This is the combined, local to world transform. It includes
  // |inheritable_transform_|, |transform_|, and anchoring adjustments.
  gfx::Transform world_space_transform_;

  ColorScheme::Mode mode_ = ColorScheme::kModeNormal;

  UiElement* parent_ = nullptr;
  std::vector<UiElement*> children_;

  DISALLOW_COPY_AND_ASSIGN(UiElement);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_H_
