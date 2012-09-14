// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jamesr): Remove or refactor this dependency.
#if INSIDE_WEBKIT_BUILD
#include "Source/WebCore/platform/graphics/UnitBezier.h"
#else
#include "third_party/WebKit/Source/WebCore/platform/graphics/UnitBezier.h"
#endif

namespace cc {
typedef WebCore::UnitBezier UnitBezier;
}
