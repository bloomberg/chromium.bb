// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_SOFTWARE_OUTPUT_DEVICE_H_
#define CC_TEST_FAKE_SOFTWARE_OUTPUT_DEVICE_H_

#include "base/logging.h"
#include "cc/software_output_device.h"
#include "skia/ext/refptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebImage.h"
#include "third_party/skia/include/core/SkDevice.h"

namespace cc {

class FakeSoftwareOutputDevice : public SoftwareOutputDevice {
 public:
  FakeSoftwareOutputDevice();
  virtual ~FakeSoftwareOutputDevice();

  virtual WebKit::WebImage* Lock(bool forWrite) OVERRIDE;
  virtual void Unlock() OVERRIDE;

  virtual void DidChangeViewportSize(gfx::Size size) OVERRIDE;

 private:
  skia::RefPtr<SkDevice> device_;
  WebKit::WebImage image_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_SOFTWARE_OUTPUT_DEVICE_H_
