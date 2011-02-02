// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines a simple integer rectangle class.  The containment semantics
// are array-like; that is, the coordinate (x, y) is considered to be
// contained by the rectangle, but the coordinate (x + width, y) is not.
// The class will happily let you create malformed rectangles (that is,
// rectangles with negative width and/or height), but there will be assertions
// in the operations (such as Contains()) to complain in this case.

#ifndef GFX_RECT_H_
#define GFX_RECT_H_
#pragma once

#include "ui/gfx/rect.h"
// TODO(sail): remove this file once all includes have been updated.

#endif  // GFX_RECT_H_
