// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SOFTWARE_OUTPUT_DEVICE_H_
#define CC_SOFTWARE_OUTPUT_DEVICE_H_

#include "third_party/WebKit/Source/Platform/chromium/public/WebImage.h"
#include "ui/gfx/size.h"

namespace cc {

// This is a "tear-off" class providing software drawing support to
// OutputSurface, such as to a platform-provided window framebuffer.
class SoftwareOutputDevice {
 public:
  virtual ~SoftwareOutputDevice() {}

  // Lock the framebuffer and return a pointer to a WebImage referring to its
  // pixels. Set forWrite if you intend to change the pixels. Readback
  // is supported whether or not forWrite is set.
  // TODO(danakj): Switch this from WebImage to a Skia type.
  virtual WebKit::WebImage* Lock(bool forWrite) = 0;
  virtual void Unlock() = 0;

  virtual void DidChangeViewportSize(gfx::Size) = 0;
};

}  // namespace cc

#endif // CC_SOFTWARE_OUTPUT_DEVICE_H_
