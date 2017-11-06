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
#include "chrome/browser/vr/databinding/binding_base.h"
#include "chrome/browser/vr/elements/draw_phase.h"
#include "chrome/browser/vr/elements/ui_element_iterator.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/target_property.h"
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

enum LayoutAlignment {
  NONE = 0,
  LEFT,
  RIGHT,
  TOP,
  BOTTOM,
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

  enum UpdatePhase {
    kDirty = 0,
    kUpdatedAnimations,
    kUpdatedBindings,
    kUpdatedComputedOpacity,
    kUpdatedTexturesAndSizes,
    kUpdatedLayout,
    kUpdatedWorldSpaceTransform,
    kClean = kUpdatedWorldSpaceTransform,
  };

  // Returns true if the element needs to be re-drawn.
  virtual bool PrepareToDraw();

  // Returns true if the element has been updated in any visible way.
  bool DoBeginFrame(const base::TimeTicks& time,
                    const gfx::Vector3dF& head_direction);

  // Indicates whether the element should be tested for cursor input.
  bool IsHitTestable() const;

  virtual void Render(UiElementRenderer* renderer,
                      const gfx::Transform& model_view_proj_matrix) const;

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

  // If true, the object has a non-zero opacity.
  bool IsVisible() const;
  // For convenience, sets opacity to |opacity_when_visible_|.
  virtual void SetVisible(bool visible);
  void SetVisibleImmediately(bool visible);

  void set_opacity_when_visible(float opacity) {
    opacity_when_visible_ = opacity;
  }
  float opacity_when_visible() const { return opacity_when_visible_; }

  bool requires_layout() const { return requires_layout_; }
  void set_requires_layout(bool requires_layout) {
    requires_layout_ = requires_layout;
  }

  bool hit_testable() const { return hit_testable_; }
  void set_hit_testable(bool hit_testable) { hit_testable_ = hit_testable; }

  bool scrollable() const { return scrollable_; }
  void set_scrollable(bool scrollable) { scrollable_ = scrollable; }

  gfx::SizeF size() const;
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

  // Returns the target value of the animation if the corresponding property is
  // being animated, or the current value otherwise.
  gfx::SizeF GetTargetSize() const;
  cc::TransformOperations GetTargetTransform() const;
  float GetTargetOpacity() const;

  float opacity() const { return opacity_; }
  virtual void SetOpacity(float opacity);

  float corner_radius() const { return corner_radius_; }
  void set_corner_radius(float corner_radius) {
    corner_radius_ = corner_radius;
  }

  float computed_opacity() const;
  void set_computed_opacity(float computed_opacity) {
    computed_opacity_ = computed_opacity;
  }

  LayoutAlignment x_anchoring() const { return x_anchoring_; }
  void set_x_anchoring(LayoutAlignment x_anchoring) {
    x_anchoring_ = x_anchoring;
  }

  LayoutAlignment y_anchoring() const { return y_anchoring_; }
  void set_y_anchoring(LayoutAlignment y_anchoring) {
    y_anchoring_ = y_anchoring;
  }

  LayoutAlignment x_centering() const { return x_centering_; }
  void set_x_centering(LayoutAlignment x_centering) {
    x_centering_ = x_centering;
  }

  LayoutAlignment y_centering() const { return y_centering_; }
  void set_y_centering(LayoutAlignment y_centering) {
    y_centering_ = y_centering;
  }

  int draw_phase() const { return draw_phase_; }
  void set_draw_phase(int draw_phase) { draw_phase_ = draw_phase; }

  const gfx::Transform& inheritable_transform() const {
    return inheritable_transform_;
  }
  void set_inheritable_transform(const gfx::Transform& transform) {
    inheritable_transform_ = transform;
  }

  UiElementName name() const { return name_; }
  void set_name(UiElementName name) { name_ = name; }

  void SetMode(ColorScheme::Mode mode);
  ColorScheme::Mode mode() const { return mode_; }

  const gfx::Transform& world_space_transform() const;
  void set_world_space_transform(const gfx::Transform& transform) {
    world_space_transform_ = transform;
  }

  // Transformations are applied relative to the parent element, rather than
  // absolutely.
  void AddChild(std::unique_ptr<UiElement> child);
  std::unique_ptr<UiElement> RemoveChild(UiElement* to_remove);
  UiElement* parent() { return parent_; }
  const UiElement* parent() const { return parent_; }

  void AddBinding(std::unique_ptr<BindingBase> binding);
  const std::vector<std::unique_ptr<BindingBase>>& bindings() {
    return bindings_;
  }

  // Return true if any bindings has updates.
  bool UpdateBindings();

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
  void NotifyClientFloatAnimated(float value,
                                 int target_property_id,
                                 cc::Animation* animation) override;
  void NotifyClientTransformOperationsAnimated(
      const cc::TransformOperations& operations,
      int target_property_id,
      cc::Animation* animation) override;
  void NotifyClientSizeAnimated(const gfx::SizeF& size,
                                int target_property_id,
                                cc::Animation* animation) override;

  void SetTransitionedProperties(const std::set<TargetProperty>& properties);
  void SetTransitionDuration(base::TimeDelta delta);

  void AddAnimation(std::unique_ptr<cc::Animation> animation);
  void RemoveAnimation(int animation_id);
  bool IsAnimatingProperty(TargetProperty property) const;

  // Handles positioning adjustments for children. This will be overridden by
  // UiElements providing custom layout modes. See the documentation of the
  // override for their particular functionality. The base implementation
  // applies anchoring.
  virtual void LayOutChildren();

  virtual gfx::Transform LocalTransform() const;

  void UpdateComputedOpacityRecursive();
  void UpdateWorldSpaceTransformRecursive();

  std::vector<std::unique_ptr<UiElement>>& children() { return children_; }
  const std::vector<std::unique_ptr<UiElement>>& children() const {
    return children_;
  }

  typedef ForwardUiElementIterator iterator;
  typedef ConstForwardUiElementIterator const_iterator;
  typedef ReverseUiElementIterator reverse_iterator;
  typedef ConstReverseUiElementIterator const_reverse_iterator;

  iterator begin() { return iterator(this); }
  iterator end() { return iterator(nullptr); }
  const_iterator begin() const { return const_iterator(this); }
  const_iterator end() const { return const_iterator(nullptr); }

  reverse_iterator rbegin() { return reverse_iterator(this); }
  reverse_iterator rend() { return reverse_iterator(nullptr); }
  const_reverse_iterator rbegin() const { return const_reverse_iterator(this); }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(nullptr);
  }

  void set_update_phase(UpdatePhase phase) { phase_ = phase; }

  // This is true for all elements that respect the given view model matrix. If
  // this is ignored (say for head-locked elements that draw in screen space),
  // then this function should return false.
  virtual bool IsWorldPositioned() const;

  std::string DebugName() const;

 protected:
  AnimationPlayer& animation_player() { return animation_player_; }

  base::TimeTicks last_frame_time() const { return last_frame_time_; }

  // This is to be used only during the texture / size updated phase (i.e., to
  // change your size based on your old size).
  gfx::SizeF stale_size() const;

 private:
  virtual void OnSetMode();
  virtual void OnUpdatedWorldSpaceTransform();

  // Returns true if the element has been updated in any visible way.
  virtual bool OnBeginFrame(const base::TimeTicks& time,
                            const gfx::Vector3dF& look_at);

  // Valid IDs are non-negative.
  int id_ = -1;

  // If false, the reticle will not hit the element, even if visible.
  bool hit_testable_ = true;

  // A signal to the input routing machinery that this element accepts scrolls.
  bool scrollable_ = false;

  // The size of the object.  This does not affect children.
  gfx::SizeF size_ = {1.0f, 1.0f};

  // The opacity of the object (between 0.0 and 1.0).
  float opacity_ = 1.0f;

  // SetVisible(true) is an alias for SetOpacity(opacity_when_visible_).
  float opacity_when_visible_ = 1.0f;

  // A signal that this element is to be considered in |LayOutChildren|.
  bool requires_layout_ = true;

  // The corner radius of the object. Analogous to the CSS property,
  // border-radius. This is in meters (same units as |size|).
  float corner_radius_ = 0.0f;

  // The computed opacity, incorporating opacity of parent objects.
  float computed_opacity_ = 1.0f;

  // If anchoring is specified, the translation will be relative to the
  // specified edge(s) of the parent, rather than the center.  A parent object
  // must be specified when using anchoring.
  LayoutAlignment x_anchoring_ = LayoutAlignment::NONE;
  LayoutAlignment y_anchoring_ = LayoutAlignment::NONE;

  // If centering is specified, the elements layout offset is adjusted such that
  // it is positioned relative to its own edge or corner, rather than center.
  LayoutAlignment x_centering_ = LayoutAlignment::NONE;
  LayoutAlignment y_centering_ = LayoutAlignment::NONE;

  AnimationPlayer animation_player_;

  int draw_phase_ = kPhaseNone;

  // This is the time as of the last call to |Animate|. It is needed when
  // reversing transitions.
  base::TimeTicks last_frame_time_;

  // This transform can be used by children to derive position of its parent.
  gfx::Transform inheritable_transform_;

  // An optional, but stable and semantic identifier for an element used in lieu
  // of a string.
  UiElementName name_ = UiElementName::kNone;

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
  std::vector<std::unique_ptr<UiElement>> children_;

  std::vector<std::unique_ptr<BindingBase>> bindings_;

  UpdatePhase phase_ = kClean;

  DISALLOW_COPY_AND_ASSIGN(UiElement);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_H_
