// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASH_LAYOUT_CONSTANTS_H_
#define ASH_ASH_LAYOUT_CONSTANTS_H_

#include "ash/ash_export.h"
#include "ui/gfx/geometry/size.h"

enum class AshLayoutSize {
  // Size of a caption button in a maximized browser window.
  kBrowserCaptionMaximized,

  // Size of a caption button in a restored browser window.
  kBrowserCaptionRestored,

  // Size of a caption button in a non-browser window.
  kNonBrowserCaption,
};

ASH_EXPORT gfx::Size GetAshLayoutSize(AshLayoutSize size);

#endif  // ASH_ASH_LAYOUT_CONSTANTS_H_
