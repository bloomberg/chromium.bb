// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/mock_android_overlay.h"

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

MockAndroidOverlay::Callbacks::Callbacks() = default;
MockAndroidOverlay::Callbacks::Callbacks(const Callbacks&) = default;
MockAndroidOverlay::Callbacks::~Callbacks() = default;

MockAndroidOverlay::MockAndroidOverlay() : weak_factory_(this) {}

MockAndroidOverlay::~MockAndroidOverlay() {}

void MockAndroidOverlay::SetConfig(const Config& config) {
  config_ = config;
}

MockAndroidOverlay::Callbacks MockAndroidOverlay::GetCallbacks() {
  Callbacks c;
  c.OverlayReady = base::Bind(&MockAndroidOverlay::OnOverlayReady,
                              weak_factory_.GetWeakPtr());
  c.OverlayFailed = base::Bind(&MockAndroidOverlay::OnOverlayFailed,
                               weak_factory_.GetWeakPtr());
  c.SurfaceDestroyed = base::Bind(&MockAndroidOverlay::OnSurfaceDestroyed,
                                  weak_factory_.GetWeakPtr());

  return c;
}

void MockAndroidOverlay::OnOverlayReady() {
  config_.ready_cb.Run(this);
}

void MockAndroidOverlay::OnOverlayFailed() {
  config_.failed_cb.Run(this);
}

void MockAndroidOverlay::OnSurfaceDestroyed() {
  config_.destroyed_cb.Run(this);
}

}  // namespace media
