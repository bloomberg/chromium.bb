// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "content/browser/compositor/software_output_device_mac.h"

#include "ui/compositor/compositor.h"

// Declare methods used to present swaps to this view.
@interface NSView (SoftwareDelegatedFrame)
- (void)gotSoftwareFrame:(cc::SoftwareFrameData*)frame_data
         withScaleFactor:(float)scale_factor
              withCanvas:(SkCanvas*)canvas;
@end

@implementation NSView (SoftwareDelegatedFrame)
- (void)gotSoftwareFrame:(cc::SoftwareFrameData*)frame_data
         withScaleFactor:(float)scale_factor
              withCanvas:(SkCanvas*)canvas {
}
@end

namespace content {

SoftwareOutputDeviceMac::SoftwareOutputDeviceMac(ui::Compositor* compositor)
    : compositor_(compositor) {
}

SoftwareOutputDeviceMac::~SoftwareOutputDeviceMac() {
}

void SoftwareOutputDeviceMac::EndPaint(cc::SoftwareFrameData* frame_data) {
  SoftwareOutputDevice::EndPaint(frame_data);

  NSView* view = compositor_->widget();
  [view gotSoftwareFrame:frame_data
         withScaleFactor:scale_factor_
              withCanvas:canvas_.get()];
}

}  // namespace content
