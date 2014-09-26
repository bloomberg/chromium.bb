// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/platform_viewport_headless.h"

namespace mojo {

// static
scoped_ptr<PlatformViewport> PlatformViewport::Create(Delegate* delegate) {
  return PlatformViewportHeadless::Create(delegate);
}

}  // namespace mojo
