// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCLayerTestCommon_h
#define CCLayerTestCommon_h

#include "CCRenderPass.h"
#include "IntRect.h"
#include "Region.h"

namespace CCLayerTestCommon {

extern const char* quadString;

void verifyQuadsExactlyCoverRect(const cc::CCQuadList&, const cc::IntRect&);

} // namespace CCLayerTestCommon
#endif // CCLayerTestCommon_h
