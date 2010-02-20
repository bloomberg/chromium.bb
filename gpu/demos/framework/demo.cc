// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/demos/framework/demo.h"
#include "gpu/demos/framework/demo_factory.h"

namespace gpu {
namespace demos {

Demo::Demo() : width_(0), height_(0) {
}

Demo::~Demo() {
}

void Demo::InitWindowSize(int width, int height) {
  width_ = width;
  height_ = height;
}

void Demo::Draw() {
  float elapsed_sec = 0.0f;
  const base::Time current_time = base::Time::Now();
  if (!last_draw_time_.is_null()) {
    base::TimeDelta time_delta = current_time - last_draw_time_;
    elapsed_sec = static_cast<float>(time_delta.InSecondsF());
  }
  last_draw_time_ = current_time;

  Render(elapsed_sec);
}

}  // namespace demos
}  // namespace gpu
