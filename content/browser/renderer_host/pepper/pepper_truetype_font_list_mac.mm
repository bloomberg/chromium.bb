// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_truetype_font_list.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/strings/sys_string_conversions.h"

namespace content {

void GetFontFamilies_SlowBlocking(std::vector<std::string>* font_families) {
  base::mac::ScopedNSAutoreleasePool autorelease_pool;
  NSFontManager* fontManager = [[[NSFontManager alloc] init] autorelease];
  NSArray* fonts = [fontManager availableFontFamilies];
  font_families->reserve([fonts count]);
  for (NSString* family_name in fonts)
    font_families->push_back(base::SysNSStringToUTF8(family_name));
}

}  // namespace content
