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

// TODO(vollick): Make every UiElement draw itself (i.e., encapsulate rendering
// logic in the elements). This will let us remove this enumeration and the
// UiRender can then just iterate over elements and tell them to render
// themselves in turn. NB: this includes the reticle and controller, which are
// not yet represented as UiElements.
enum Fill {
  NONE = 0,
  // The element is filled with a radial gradient as specified by the edge and
  // center color.
  OPAQUE_GRADIENT = 1,
  // Same as OPAQUE_GRADIENT but the element is drawn as a grid.
  GRID_GRADIENT = 2,
  // The element draws itself.
  SELF = 3,
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

  // If true, transformations will be applied relative to the field of view,
  // rather than the world.
  bool lock_to_fov() const { return lock_to_fov_; }
  void set_lock_to_fov(bool lock) { lock_to_fov_ = lock; }

  // If true should be drawn in the world viewport, but over all other elements.
  bool is_overlay() const { return is_overlay_; }
  void set_is_overlay(bool is_overlay) { is_overlay_ = is_overlay; }

  bool scrollable() const { return scrollable_; }
  void set_scrollable(bool scrollable) { scrollable_ = scrollable; }

  // The computed lock to the FoV, incorporating lock of parent objects.
  bool computed_lock_to_fov() const { return computed_lock_to_fov_; }
  void set_computed_lock_to_fov(bool computed_lock) {
    computed_lock_to_fov_ = computed_lock;
  }

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

  Fill fill() const { return fill_; }
  void set_fill(Fill fill) { fill_ = fill; }

  SkColor edge_color() const { return edge_color_; }
  void set_edge_color(const SkColor& edge_color) { edge_color_ = edge_color; }

  SkColor center_color() const { return center_color_; }
  void set_center_color(const SkColor& center_color) {
    center_color_ = center_color;
  }

  SkColor grid_color() const { return grid_color_; }
  void set_grid_color(const SkColor& grid_color) { grid_color_ = grid_color; }

  int gridline_count() const { return gridline_count_; }
  void set_gridline_count(int gridline_count) {
    gridline_count_ = gridline_count;
  }

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

 protected:
  virtual void OnSetMode();

  std::vector<UiElement*>& children() { return children_; }

 private:
  // Valid IDs are non-negative.
  int id_ = -1;

  // If true, this object will be visible.
  bool visible_ = false;

  // If false, the reticle will not hit the element, even if visible.
  bool hit_testable_ = true;

  // If true, transformations will be applied relative to the field of view,
  // rather than the world.
  bool lock_to_fov_ = false;

  // The computed lock to the FoV, incorporating lock of parent objects.
  bool computed_lock_to_fov_ = false;

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

  Fill fill_ = Fill::NONE;

  SkColor edge_color_ = SK_ColorWHITE;
  SkColor center_color_ = SK_ColorWHITE;
  SkColor grid_color_ = SK_ColorWHITE;

  int gridline_count_ = 1;

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
