// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/font_descriptor_mac.h"

#include <Cocoa/Cocoa.h>

#include "base/sys_string_conversions.h"

FontDescriptor::FontDescriptor(NSFont* font) {
  font_name = base::SysNSStringToUTF16([font fontName]);
  font_point_size = [font pointSize];
}

NSFont* FontDescriptor::nsFont() const {
  NSString* font_name_ns = base::SysUTF16ToNSString(font_name);
  NSFont* font = [NSFont fontWithName:font_name_ns size:font_point_size];
  return font;
}
