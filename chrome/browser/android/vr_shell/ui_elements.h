// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_H_

#include <cmath>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/android/vr_shell/vr_math.h"
#include "third_party/gvr-android-sdk/src/ndk-beta/include/vr/gvr/capi/include/gvr_types.h"

namespace vr_shell {

class Animation;

enum XAnchoring {
  XLEFT = 0,
  XRIGHT,
  XCENTER,
  XNONE
};

enum YAnchoring {
  YTOP = 0,
  YBOTTOM,
  YCENTER,
  YNONE
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

  int id = 0;

  // samplerExternalOES texture data for desktop content image.
  int content_texture_handle = -1;
  Rectf copy_rect = {0.0f, 0.0f, 0.0f, 0.0f};
  Recti window_rect = {0.0f, 0.0f, 0.0f, 0.0f};
  gvr::Vec3f size = {0.0f, 0.0f, 0.0f};
  gvr::Vec3f translation = {0.0f, 0.0f, 0.0f};
  XAnchoring x_anchoring = XAnchoring::XNONE;
  YAnchoring y_anchoring = YAnchoring::YNONE;
  bool anchor_z = false;
  std::vector<float> orientation_axis_angle;
  std::vector<float> rotation_axis_angle;
  std::vector<std::unique_ptr<Animation>> animations;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentRectangle);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_H_
