// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_FONT_DESCRIPTOR_MAC_H_
#define CHROME_COMMON_FONT_DESCRIPTOR_MAC_H_

#include "base/string16.h"

#ifdef __OBJC__
@class NSFont;
#else
class NSFont;
#endif

// Container to allow serializing an NSFont over IPC.
struct FontDescriptor {
  explicit FontDescriptor(NSFont* font);

  FontDescriptor() : font_point_size(0) {}
  
  // Return an autoreleased NSFont corresponding to the font description.
  NSFont* nsFont() const;

  // Name of the font.
  string16 font_name;
  
  // Size in points.
  float font_point_size;
};

#endif  // CHROME_COMMON_FONT_DESCRIPTOR_MAC_H_
