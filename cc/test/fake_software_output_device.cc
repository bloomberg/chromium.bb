// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_software_output_device.h"

namespace cc {

FakeSoftwareOutputDevice::FakeSoftwareOutputDevice() {}

FakeSoftwareOutputDevice::~FakeSoftwareOutputDevice() {}

WebKit::WebImage* FakeSoftwareOutputDevice::Lock(bool forWrite) {
  DCHECK(device_);
  image_ = device_->accessBitmap(forWrite);
  return &image_;
}

void FakeSoftwareOutputDevice::Unlock() {
  image_.reset();
}

void FakeSoftwareOutputDevice::DidChangeViewportSize(gfx::Size size) {
  if (device_.get() &&
      size.width() == device_->width() &&
      size.height() == device_->height())
    return;

  device_ = skia::AdoptRef(new SkDevice(
      SkBitmap::kARGB_8888_Config, size.width(), size.height(), true));
}

}  // namespace cc
