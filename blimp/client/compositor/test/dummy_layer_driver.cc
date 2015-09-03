// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/compositor/test/dummy_layer_driver.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "cc/layers/layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/trees/layer_tree_settings.h"
#include "ui/gfx/geometry/size.h"

namespace blimp {

DummyLayerDriver::DummyLayerDriver()
    : layer_(cc::SolidColorLayer::Create(cc::LayerSettings())),
      animation_running_(false),
      weak_factory_(this) {
  layer_->SetIsDrawable(true);
  layer_->SetBackgroundColor(SK_ColorBLUE);
  layer_->SetBounds(gfx::Size(300, 300));
}

DummyLayerDriver::~DummyLayerDriver() {}

void DummyLayerDriver::SetParentLayer(scoped_refptr<cc::Layer> layer) {
  // This will automatically remove layer_ from the previous layer.
  layer->AddChild(layer_);

  // If we're not expecting another animation step, start one now.
  if (!animation_running_) {
    animation_running_ = true;
    StepAnimation();
  }
}

void DummyLayerDriver::StepAnimation() {
  // Detached from the cc::LayerTreeHost. Stop doing work.
  if (!layer_->layer_tree_host()) {
    animation_running_ = false;
    return;
  }

  // Fun with colors
  SkColor color = layer_->background_color();
  color = SkColorSetRGB((SkColorGetR(color) + 5) % 255,
                        (SkColorGetG(color) + 3) % 255,
                        (SkColorGetB(color) + 1) % 255);
  layer_->SetBackgroundColor(color);

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DummyLayerDriver::StepAnimation, weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(16));
}

}  // namespace blimp
