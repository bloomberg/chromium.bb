// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_PARENT_CONTEXT_PROVIDER_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_PARENT_CONTEXT_PROVIDER_H_

#include "base/basictypes.h"

namespace content {
class WebGraphicsContext3DCommandBufferImpl;

// Defines the mechanism by which a Pepper 3D context fetches its
// parent context for display to the screen.
class PepperParentContextProvider {
 public:
  virtual WebGraphicsContext3DCommandBufferImpl*
      GetParentContextForPlatformContext3D() = 0;

 protected:
  PepperParentContextProvider();
  virtual ~PepperParentContextProvider();

 private:
  DISALLOW_COPY_AND_ASSIGN(PepperParentContextProvider);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_PARENT_CONTEXT_PROVIDER_H_
