// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_VR_VR_TRANSFORM_UTILS_H
#define CONTENT_BROWSER_VR_VR_TRANSFORM_UTILS_H

#include "content/common/vr_service.mojom.h"

namespace content {

VRVector4Ptr MatrixToOrientationQuaternion(
    const float m0[3], const float m1[3], const float m2[3]);

}  // namespace content

#endif  // CONTENT_BROWSER_VR_VR_TRANSFORM_UTILS_H
