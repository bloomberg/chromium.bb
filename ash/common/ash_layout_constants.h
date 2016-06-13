// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_ASH_LAYOUT_CONSTANTS_H_
#define ASH_COMMON_ASH_LAYOUT_CONSTANTS_H_

#include "ash/ash_export.h"
#include "ui/gfx/geometry/size.h"

enum class AshLayoutSize {
  // Size of a caption button in a maximized browser window.
  BROWSER_MAXIMIZED_CAPTION_BUTTON,

  // Size of a caption button in a restored browser window.
  BROWSER_RESTORED_CAPTION_BUTTON,

  // Size of a caption button in a non-browser window.
  NON_BROWSER_CAPTION_BUTTON,
};

ASH_EXPORT gfx::Size GetAshLayoutSize(AshLayoutSize size);

#endif  // ASH_COMMON_ASH_LAYOUT_CONSTANTS_H_
