// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENT_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "device/vr/vr_types.h"

namespace base {
class TimeTicks;
}

namespace vr_shell {

class Animation;

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

enum Fill {
  NONE = 0,
  // The element is filled with part of the HTML UI as specified by the copy
  // rect.
  SPRITE = 1,
  // The element is filled with a radial gradient as specified by the edge and
  // center color.
  OPAQUE_GRADIENT = 2,
  // Same as OPAQUE_GRADIENT but the element is drawn as a grid.
  GRID_GRADIENT = 3,
  // The element is filled with the content web site. Only one content element
  // may be added to the
  // scene.
  CONTENT = 4,
};

struct Transform {
  Transform();

  void MakeIdentity();
  void Rotate(const vr::Quatf& quat);
  void Rotate(const vr::RotationAxisAngle& axis_angle);
  void Translate(const gfx::Vector3dF& translation);
  void Scale(const gfx::Vector3dF& scale);

  vr::Mat4f to_world;
};

class WorldRectangle {
 public:
  const vr::Mat4f& TransformMatrix() const;
  Transform* mutable_transform() { return &transform_; }

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
  gfx::PointF GetUnitRectangleCoordinates(const gfx::Point3F& world_point);

 private:
  Transform transform_;
};

struct UiElement : public WorldRectangle {
  UiElement();
  ~UiElement();

  void Animate(const base::TimeTicks& time);

  // Indicates whether the element should be visually rendered.
  bool IsVisible() const;

  // Indicates whether the element should be tested for cursor input.
  bool IsHitTestable() const;

  // Valid IDs are non-negative.
  int id = -1;

  // Name string for debugging and testing purposes.
  std::string name;

  // If a non-negative parent ID is specified, applicable transformations
  // are applied relative to the parent, rather than absolutely.
  int parent_id = -1;

  // If true, this object will be visible.
  bool visible = true;

  // If false, the reticle will not hit the element, even if visible.
  bool hit_testable = true;

  // If true, transformations will be applied relative to the field of view,
  // rather than the world.
  bool lock_to_fov = false;

  // Specifies the region (in pixels) of a texture to render.
  gfx::Rect copy_rect = {0, 0, 0, 0};

  // The size of the object.  This does not affect children.
  gfx::Vector3dF size = {1.0f, 1.0f, 1.0f};

  // The scale of the object, and its children.
  gfx::Vector3dF scale = {1.0f, 1.0f, 1.0f};

  // The rotation of the object, and its children.
  vr::RotationAxisAngle rotation = {1.0f, 0.0f, 0.0f, 0.0f};

  // The translation of the object, and its children.  Translation is applied
  // after rotation and scaling.
  gfx::Vector3dF translation = {0.0f, 0.0f, 0.0f};

  // The opacity of the object (between 0.0 and 1.0).
  float opacity = 1.0f;

  // The computed opacity, incorporating opacity of parent objects.
  float computed_opacity;

  // The computed lock to the FoV, incorporating lock of parent objects.
  bool computed_lock_to_fov;

  // If anchoring is specified, the translation will be relative to the
  // specified edge(s) of the parent, rather than the center.  A parent object
  // must be specified when using anchoring.
  XAnchoring x_anchoring = XAnchoring::XNONE;
  YAnchoring y_anchoring = YAnchoring::YNONE;

  // Animations that affect the properties of the object over time.
  std::vector<std::unique_ptr<Animation>> animations;

  Fill fill = Fill::NONE;

  vr::Colorf edge_color = {1.0f, 1.0f, 1.0f, 1.0f};
  vr::Colorf center_color = {1.0f, 1.0f, 1.0f, 1.0f};

  int gridline_count = 1;

  int draw_phase = 1;

  // This transform can be used by children to derive position of its parent.
  Transform inheritable_transform;

  // A flag usable during transformation calculates to avoid duplicate work.
  bool dirty;

 private:
  DISALLOW_COPY_AND_ASSIGN(UiElement);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENT_H_
