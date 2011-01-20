// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "app/app_switches.h"

namespace switches {

// Stop the GPU from synchronizing on the vsync before presenting.
const char kDisableGpuVsync[]               = "disable-gpu-vsync";

// Select which implementation of GL the GPU process should use. Options are:
//  desktop: whatever desktop OpenGL the user has installed (Linux and Mac
//           default).
//  egl: whatever EGL / GLES2 the user has installed (Windows default - actually
//       ANGLE).
//  osmesa: The OSMesa software renderer.
const char kUseGL[]                         = "use-gl";

}  // namespace switches
