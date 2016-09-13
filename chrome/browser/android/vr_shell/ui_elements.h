// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_H_

#include <cmath>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/android/vr_shell/vr_util.h"
#include "third_party/gvr-android-sdk/src/ndk-beta/include/vr/gvr/capi/include/gvr_types.h"

namespace vr_shell {

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

  int id;
  // samplerExternalOES texture data for desktop content image.
  int content_texture_handle;
  Rectf copy_rect;
  Recti window_rect;
  gvr::Vec3f size;
  gvr::Vec3f translation;
  XAnchoring x_anchoring;
  YAnchoring y_anchoring;
  bool anchor_z;
  std::vector<float> orientation_axis_angle;
  std::vector<float> rotation_axis_angle;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentRectangle);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_H_
