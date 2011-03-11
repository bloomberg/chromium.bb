// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/font_descriptor_mac.h"

#include <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class FontSerializationTest : public PlatformTest {};


// Compare 2 fonts, make sure they point at the same font definition and have
// the same style.  Only Bold & Italic style attributes are tested since those
// are the only ones we care about at the moment.
bool CompareFonts(NSFont* font1, NSFont* font2) {
  ATSFontRef id1 = CTFontGetPlatformFont(reinterpret_cast<CTFontRef>(font1), 0);
  ATSFontRef id2 = CTFontGetPlatformFont(reinterpret_cast<CTFontRef>(font2), 0);
  
  if (id1 != id2) {
    LOG(ERROR) << "ATSFontRefs for "
        << [[font1 fontName] UTF8String]
        << " and "
        << [[font2 fontName] UTF8String]
        << " are different";
    return false;
  }
  
  CGFloat size1 = [font1 pointSize];
  CGFloat size2 = [font2 pointSize];
  if (size1 != size2) {
    LOG(ERROR) << "font sizes for "
        << [[font1 fontName] UTF8String] << " (" << size1 << ")"
        << "and"
        << [[font2 fontName] UTF8String]  << " (" << size2 << ")"
        << " are different";
    return false;
  }

  NSFontTraitMask traits1 = [[NSFontManager sharedFontManager]
                                traitsOfFont:font1];
  NSFontTraitMask traits2 = [[NSFontManager sharedFontManager]
                                traitsOfFont:font2];
  
  bool is_bold1 = traits1 & NSBoldFontMask;
  bool is_bold2 = traits2 & NSBoldFontMask;
  bool is_italic1 = traits1 & NSItalicFontMask;
  bool is_italic2 = traits2 & NSItalicFontMask;
  
  if (is_bold1 != is_bold2 || is_italic1 != is_italic2) {
    LOG(ERROR) << "Style information for "
        << [[font1 fontName] UTF8String]
        << " and "
        << [[font2 fontName] UTF8String]
        << " are different";
        return false;
  }
  
  return true;
}

// Verify that serialization and deserialization of fonts with various styles
// is performed correctly by FontDescriptor.
TEST_F(FontSerializationTest, StyledFonts) {
  NSFont* plain_font = [NSFont systemFontOfSize:12.0];
  ASSERT_TRUE(plain_font != nil);
  FontDescriptor desc_plain(plain_font);
  EXPECT_TRUE(CompareFonts(plain_font, desc_plain.nsFont()));
  
  NSFont* bold_font = [NSFont boldSystemFontOfSize:30.0];
  ASSERT_TRUE(bold_font != nil);
  FontDescriptor desc_bold(bold_font);
  EXPECT_TRUE(CompareFonts(bold_font, desc_bold.nsFont()));
  
  NSFont* italic_bold_font =
      [[NSFontManager sharedFontManager]
          fontWithFamily:@"Courier"
                  traits:(NSBoldFontMask | NSItalicFontMask)
                  weight:5
                    size:18.0];
  ASSERT_TRUE(italic_bold_font != nil);
  FontDescriptor desc_italic_bold(italic_bold_font);
  EXPECT_TRUE(CompareFonts(italic_bold_font, desc_italic_bold.nsFont()));
}

}  // namsepace
