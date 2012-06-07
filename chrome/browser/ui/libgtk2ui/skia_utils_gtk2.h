// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTK2UI_SKIA_UTILS_GTK2_H_
#define CHROME_BROWSER_UI_LIBGTK2UI_SKIA_UTILS_GTK2_H_
#pragma once

#include "third_party/skia/include/core/SkColor.h"

typedef struct _GdkColor GdkColor;

namespace libgtk2ui {

// Converts GdkColors to the ARGB layout Skia expects.
SkColor GdkColorToSkColor(GdkColor color);

// Converts ARGB to GdkColor.
GdkColor SkColorToGdkColor(SkColor color);

}  // namespace libgtk2ui

#endif  // CHROME_BROWSER_UI_LIBGTK2UI_SKIA_UTILS_GTK2_H_
