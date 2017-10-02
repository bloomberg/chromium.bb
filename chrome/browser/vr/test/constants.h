// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_CONSTANTS_H_
#define CHROME_BROWSER_VR_TEST_CONSTANTS_H_

#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/transform.h"

namespace vr {

// Proj matrix as used on a Pixel phone.
static const gfx::Transform kProjMatrix(1.03317f,
                                        0.0f,
                                        0.271253f,
                                        0.0f,
                                        0.0f,
                                        0.862458f,
                                        -0.0314586f,
                                        0.0f,
                                        0.0f,
                                        0.0f,
                                        -1.002f,
                                        -0.2002f,
                                        0.0f,
                                        0.0f,
                                        -1.0f,
                                        0.0f);

static const gfx::Vector3dF kForwardVector(0.0f, 0.0f, -1.0f);

constexpr float kEpsilon = 1e-5f;

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_CONSTANTS_H_
