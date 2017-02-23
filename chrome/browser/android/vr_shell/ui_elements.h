// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/android/vr_shell/vr_math.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

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

struct ReversibleTransform {
  ReversibleTransform();

  void MakeIdentity();
  void Rotate(gvr::Quatf quat);
  void Rotate(float ax, float ay, float az, float rad);
  void Translate(float tx, float ty, float tz);
  void Scale(float sx, float sy, float sz);

  gvr::Mat4f to_world;
  gvr::Mat4f from_world;

  // This object-to-world orientation quaternion is technically
  // redundant, but it's easy to track it here for use as needed.
  // TODO(klausw): use this instead of MatrixVectorRotation()?
  // Would need quat * vector implementation.
  gvr::Quatf orientation = {0.0f, 0.0f, 0.0f, 1.0f};
};

struct WorldObject {
  ReversibleTransform transform;
};

struct WorldRectangle : public WorldObject {
  gvr::Vec3f GetCenter() const;
  gvr::Vec3f GetNormal() const;
  float GetRayDistance(gvr::Vec3f rayOrigin, gvr::Vec3f rayVector) const;
};

struct ContentRectangle : public WorldRectangle {
  ContentRectangle();
  ~ContentRectangle();

  void Animate(int64_t time);

  // Indicates whether the element should be visually rendered.
  bool IsVisible() const;

  // Indicates whether the element should be tested for cursor input.
  bool IsHitTestable() const;

  // Valid IDs are non-negative.
  int id = -1;

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
  Recti copy_rect = {0, 0, 0, 0};

  // The size of the object.  This does not affect children.
  gvr::Vec3f size = {1.0f, 1.0f, 1.0f};

  // The scale of the object, and its children.
  gvr::Vec3f scale = {1.0f, 1.0f, 1.0f};

  // The rotation of the object, and its children.
  RotationAxisAngle rotation = {1.0f, 0.0f, 0.0f, 0.0f};

  // The translation of the object, and its children.  Translation is applied
  // after rotation and scaling.
  gvr::Vec3f translation = {0.0f, 0.0f, 0.0f};

  // The opacity of the object (between 0.0 and 1.0).
  float opacity = 1.0f;

  // The computed opacity, incorporating opacity of parent objects.
  float computed_opacity;

  // If anchoring is specified, the translation will be relative to the
  // specified edge(s) of the parent, rather than the center.  A parent object
  // must be specified when using anchoring.
  XAnchoring x_anchoring = XAnchoring::XNONE;
  YAnchoring y_anchoring = YAnchoring::YNONE;

  // Animations that affect the properties of the object over time.
  std::vector<std::unique_ptr<Animation>> animations;

  Fill fill = Fill::NONE;

  Colorf edge_color = {1.0f, 1.0f, 1.0f, 1.0f};
  Colorf center_color = {1.0f, 1.0f, 1.0f, 1.0f};

  int gridline_count = 1;

  int draw_phase = 1;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentRectangle);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_H_
