// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/demos/framework/demo.h"
#include "gpu/demos/framework/demo_factory.h"

namespace gpu {
namespace demos {

Demo::Demo() : width_(0), height_(0), last_draw_time_(0) {
}

Demo::~Demo() {
}

void Demo::Draw() {
  float elapsed_sec = 0.0f;
  clock_t current_time = clock();
  if (last_draw_time_ != 0) {
    elapsed_sec = static_cast<float>(current_time - last_draw_time_) /
        CLOCKS_PER_SEC;
  }
  last_draw_time_ = current_time;

  Render(elapsed_sec);
}

bool Demo::IsAnimated() {
  return false;
}

void Demo::Resize(int width, int height) {
  width_ = width;
  height_ = height;
}

}  // namespace demos
}  // namespace gpu
