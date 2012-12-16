// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_web_compositor_software_output_device.h"

namespace cc {

FakeWebCompositorSoftwareOutputDevice::FakeWebCompositorSoftwareOutputDevice() {
}

FakeWebCompositorSoftwareOutputDevice::
    ~FakeWebCompositorSoftwareOutputDevice() {
}

WebKit::WebImage* FakeWebCompositorSoftwareOutputDevice::lock(bool forWrite) {
  DCHECK(m_device.get());
  m_image = m_device->accessBitmap(forWrite);
  return &m_image;
}

void FakeWebCompositorSoftwareOutputDevice::unlock() {
  m_image.reset();
}

void FakeWebCompositorSoftwareOutputDevice::didChangeViewportSize(
    WebKit::WebSize size) {
  if (m_device.get() && size.width == m_device->width() &&
      size.height == m_device->height())
    return;

  m_device.reset(new SkDevice(
      SkBitmap::kARGB_8888_Config,
      size.width,
      size.height,
      true));
}

}  // namespace cc
