// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_H_
#define CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_H_

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "cc/animation/animation_target.h"
#include "cc/animation/transform_operations.h"
#include "chrome/browser/vr/animation_player.h"
#include "chrome/browser/vr/databinding/binding_base.h"
#include "chrome/browser/vr/elements/corner_radii.h"
#include "chrome/browser/vr/elements/draw_phase.h"
#include "chrome/browser/vr/elements/ui_element_iterator.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/elements/ui_element_type.h"
#include "chrome/browser/vr/model/camera_model.h"
#include "chrome/browser/vr/target_property.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/rect_f.h"
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
class SkiaSurfaceProvider;
class UiElementRenderer;
struct CameraModel;
struct TextInputInfo;

enum LayoutAlignment {
  NONE = 0,
  LEFT,
  RIGHT,
  TOP,
  BOTTOM,
};

struct EventHandlers {
  EventHandlers();
  ~EventHandlers();
  base::Callback<void()> hover_enter;
  base::Callback<void()> hover_leave;
  base::Callback<void(const gfx::PointF&)> hover_move;
  base::Callback<void()> button_down;
  base::Callback<void()> button_up;
};

struct HitTestRequest {
  gfx::Point3F ray_origin;
  gfx::Point3F ray_target;
  float max_distance_to_plane;
};

// The result of performing a hit test.
struct HitTestResult {
  enum Type {
    // The given ray does not pass through the element.
    kNone = 0,
    // The given ray does not pass through the element, but passes through the
    // element's plane.
    kHitsPlane,
    // The given ray passes through the element.
    kHits,
  };

  Type type;
  // The fields below are not set if the result Type is kNone.
  // The hit position in the element's local coordinate space.
  gfx::PointF local_hit_point;
  // The hit position relative to the world.
  gfx::Point3F hit_point;
  // The distance from the ray origin to the hit position.
  float distance_to_plane;
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
    kUpdatedBindings,
    kUpdatedAnimations,
    kUpdatedComputedOpacity,
    kUpdatedTexturesAndSizes,
    kUpdatedLayout,
    kUpdatedWorldSpaceTransform,
    kClean = kUpdatedWorldSpaceTransform,
  };

  UiElementName name() const { return name_; }
  void SetName(UiElementName name);
  virtual void OnSetName();

  UiElementName owner_name_for_test() const { return owner_name_for_test_; }
  void set_owner_name_for_test(UiElementName name) {
    owner_name_for_test_ = name;
  }

  UiElementType type() const { return type_; }
  void SetType(UiElementType type);
  virtual void OnSetType();

  DrawPhase draw_phase() const { return draw_phase_; }
  void SetDrawPhase(DrawPhase draw_phase);
  virtual void OnSetDrawPhase();

  // Returns true if the element needs to be re-drawn.
  virtual bool PrepareToDraw();

  // Returns true if the element has been updated in any visible way.
  bool DoBeginFrame(const base::TimeTicks& time,
                    const gfx::Vector3dF& head_direction);

  // Indicates whether the element should be tested for cursor input.
  bool IsHitTestable() const;

  virtual void Render(UiElementRenderer* renderer,
                      const CameraModel& model) const;

  virtual void Initialize(SkiaSurfaceProvider* provider);

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
  virtual bool LocalHitTest(const gfx::PointF& point) const;

  // Performs a hit test for the ray supplied in the request and populates the
  // result. The ray is in the world coordinate space.
  virtual void HitTest(const HitTestRequest& request,
                       HitTestResult* result) const;

  int id() const { return id_; }

  // If true, the object has a non-zero opacity.
  bool IsVisible() const;
  // For convenience, sets opacity to |opacity_when_visible_|.
  virtual void SetVisible(bool visible);
  virtual void SetVisibleImmediately(bool visible);

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

  bool focusable() const { return focusable_; }
  void set_focusable(bool focusable);
  virtual void OnSetFocusable();

  bool scrollable() const { return scrollable_; }
  void set_scrollable(bool scrollable) { scrollable_ = scrollable; }

  bool bubble_events() const { return bubble_events_; }
  void set_bubble_events(bool bubble_events) { bubble_events_ = bubble_events; }

  void set_event_handlers(const EventHandlers& event_handlers) {
    event_handlers_ = event_handlers;
  }

  // Editable elements should override these functions.
  virtual void OnFocusChanged(bool focused);
  virtual void OnInputEdited(const TextInputInfo& info);
  virtual void OnInputCommitted(const TextInputInfo& info);

  gfx::SizeF size() const;
  void SetSize(float width, float hight);
  virtual void OnSetSize(const gfx::SizeF& size);

  gfx::PointF local_origin() const { return local_origin_; }

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

  CornerRadii corner_radii() const { return corner_radii_; }
  void SetCornerRadii(const CornerRadii& radii);
  virtual void OnSetCornerRadii(const CornerRadii& radii);

  float corner_radius() const {
    DCHECK(corner_radii_.AllEqual());
    return corner_radii_.upper_left;
  }

  // Syntax sugar for setting all corner radii to the same value.
  void set_corner_radius(float corner_radius) {
    SetCornerRadii(
        {corner_radius, corner_radius, corner_radius, corner_radius});
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

  bool bounds_contain_children() const { return bounds_contain_children_; }
  void set_bounds_contain_children(bool bounds_contain_children) {
    bounds_contain_children_ = bounds_contain_children;
  }

  bool contributes_to_parent_bounds() const {
    return contributes_to_parent_bounds_;
  }
  void set_contributes_to_parent_bounds(bool value) {
    contributes_to_parent_bounds_ = value;
  }

  float x_padding() const { return x_padding_; }
  float y_padding() const { return y_padding_; }
  void set_padding(float x_padding, float y_padding) {
    x_padding_ = x_padding;
    y_padding_ = y_padding;
  }

  const gfx::Transform& inheritable_transform() const {
    return inheritable_transform_;
  }
  void set_inheritable_transform(const gfx::Transform& transform) {
    inheritable_transform_ = transform;
  }

  const gfx::Transform& world_space_transform() const;
  void set_world_space_transform(const gfx::Transform& transform) {
    world_space_transform_ = transform;
  }

  gfx::Transform ComputeTargetWorldSpaceTransform() const;
  float ComputeTargetOpacity() const;

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

  void UpdateBindings();

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

  void DoLayOutChildren();

  // Handles positioning adjustments for children. This will be overridden by
  // UiElements providing custom layout modes. See the documentation of the
  // override for their particular functionality. The base implementation
  // applies anchoring.
  virtual void LayOutChildren();

  virtual gfx::Transform LocalTransform() const;
  virtual gfx::Transform GetTargetLocalTransform() const;

  void UpdateComputedOpacity();
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

  bool updated_bindings_this_frame() const {
    return updated_bindings_this_frame_;
  }

  bool updated_visiblity_this_frame() const {
    return updated_visibility_this_frame_;
  }

  std::string DebugName() const;

  // Writes a pretty-printed version of the UiElement subtree to |os|. The
  // vector of counts represents where each ancestor on the ancestor chain is
  // situated in its parent's list of children. This is used to determine
  // whether each ancestor is the last child (which affects the lines we draw in
  // the tree).
  void DumpHierarchy(std::vector<size_t> counts, std::ostringstream* os) const;
  virtual void DumpGeometry(std::ostringstream* os) const;

 protected:
  AnimationPlayer& animation_player() { return animation_player_; }

  base::TimeTicks last_frame_time() const { return last_frame_time_; }

  // This is to be used only during the texture / size updated phase (i.e., to
  // change your size based on your old size).
  gfx::SizeF stale_size() const;

 private:
  virtual void OnUpdatedWorldSpaceTransform();

  // Returns true if the element has been updated in any visible way.
  virtual bool OnBeginFrame(const base::TimeTicks& time,
                            const gfx::Vector3dF& look_at);

  // Valid IDs are non-negative.
  int id_ = -1;

  // If false, the reticle will not hit the element, even if visible.
  bool hit_testable_ = true;

  // If false, clicking on the element doesn't give it focus.
  bool focusable_ = true;

  // A signal to the input routing machinery that this element accepts scrolls.
  bool scrollable_ = false;

  // If true, events such as OnButtonDown, OnBubbleUp, etc, get bubbled up the
  // parent chain.
  bool bubble_events_ = false;

  // The size of the object.  This does not affect children.
  gfx::SizeF size_;

  // The local orgin of the element. This can be updated, say, so that an
  // element can contain its children, even if they are not centered about its
  // true origin.
  gfx::PointF local_origin_ = {0.0f, 0.0f};

  // The opacity of the object (between 0.0 and 1.0).
  float opacity_ = 1.0f;

  // SetVisible(true) is an alias for SetOpacity(opacity_when_visible_).
  float opacity_when_visible_ = 1.0f;

  // A signal that this element is to be considered in |LayOutChildren|.
  bool requires_layout_ = true;

  // The corner radius of the object. Analogous to the CSS property,
  // border-radius. This is in meters (same units as |size|).
  CornerRadii corner_radii_ = {0, 0, 0, 0};

  // The computed opacity, incorporating opacity of parent objects.
  float computed_opacity_ = 1.0f;

  // Returns true if the last call to UpdateBindings had any effect.
  bool updated_bindings_this_frame_ = false;

  // Return true if the last call to UpdateComputedOpacity had any effect on
  // visibility.
  bool updated_visibility_this_frame_ = false;

  // If anchoring is specified, the translation will be relative to the
  // specified edge(s) of the parent, rather than the center.  A parent object
  // must be specified when using anchoring.
  LayoutAlignment x_anchoring_ = LayoutAlignment::NONE;
  LayoutAlignment y_anchoring_ = LayoutAlignment::NONE;

  // If centering is specified, the elements layout offset is adjusted such that
  // it is positioned relative to its own edge or corner, rather than center.
  LayoutAlignment x_centering_ = LayoutAlignment::NONE;
  LayoutAlignment y_centering_ = LayoutAlignment::NONE;

  // If this is true, after laying out descendants, this element updates its
  // size to accommodate all descendants, adding in the padding below along the
  // x and y axes.
  bool bounds_contain_children_ = false;
  bool contributes_to_parent_bounds_ = true;
  float x_padding_ = 0.0f;
  float y_padding_ = 0.0f;

  AnimationPlayer animation_player_;

  DrawPhase draw_phase_ = kPhaseNone;

  // This is the time as of the last call to |Animate|. It is needed when
  // reversing transitions.
  base::TimeTicks last_frame_time_;

  // This transform can be used by children to derive position of its parent.
  gfx::Transform inheritable_transform_;

  // An optional, but stable and semantic identifier for an element used in lieu
  // of a string.
  UiElementName name_ = UiElementName::kNone;

  // This name is used in tests and debugging output to associate a "component"
  // element with its logical owner, such as a button icon within a specific,
  // named button instance.
  UiElementName owner_name_for_test_ = UiElementName::kNone;

  // An optional identifier to categorize a reusable element, such as a button
  // background. It can also be used to identify categories of element for
  // common styling. Eg, applying a corner-radius to all tab thumbnails.
  UiElementType type_ = UiElementType::kTypeNone;

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

  UiElement* parent_ = nullptr;
  std::vector<std::unique_ptr<UiElement>> children_;

  std::vector<std::unique_ptr<BindingBase>> bindings_;

  UpdatePhase phase_ = kClean;

  EventHandlers event_handlers_;

  DISALLOW_COPY_AND_ASSIGN(UiElement);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_H_
