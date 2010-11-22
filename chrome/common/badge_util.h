// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_BADGE_UTIL_H_
#define CHROME_COMMON_BADGE_UTIL_H_
#pragma once

class SkPaint;

// badge_util provides a set of helper routines for rendering dynamically
// generated text overlays ("badges") on toolbar icons.
namespace badge_util {

// Helper routine that returns a singleton SkPaint object configured for
// rendering badge overlay text (correct font, typeface, etc).
SkPaint* GetBadgeTextPaintSingleton();

}  // namespace badge_util;

#endif  // CHROME_COMMON_BADGE_UTIL_H_
