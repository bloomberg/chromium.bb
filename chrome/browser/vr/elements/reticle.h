// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_RETICLE_H_
#define CHROME_BROWSER_VR_ELEMENTS_RETICLE_H_

#include "chrome/browser/vr/elements/ui_element.h"
#include "ui/gfx/geometry/point3_f.h"

namespace vr {

class UiScene;
struct Model;

class Reticle : public UiElement {
 public:
  Reticle(UiScene* scene, Model* model);
  ~Reticle() override;

 private:
  void Render(UiElementRenderer* renderer,
              const gfx::Transform& model_view_proj_matrix) const final;

  gfx::Point3F origin_;
  gfx::Point3F target_;

  // This is used to convert the target id into a UiElement instance. We are not
  // permitted to retain pointers to UiElements since they may be destructed,
  // but the scene itself is constant, so we will look up our elements on the
  // fly.
  UiScene* scene_;

  // Unlike other UiElements which bind their values form the model, the reticle
  // must derive values from the model late in the pipeline after the scene has
  // fully updated its geometry. We therefore retain a pointer to the model and
  // make use of it in |Render|.
  Model* model_;

  DISALLOW_COPY_AND_ASSIGN(Reticle);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_RETICLE_H_
