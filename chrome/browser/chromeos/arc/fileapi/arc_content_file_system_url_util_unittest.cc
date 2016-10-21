// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_url_util.h"
#include "content/public/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace arc {

TEST(ArcContentFileSystemUrlUtilTest, EncodeAndDecode) {
  {
    GURL src("file://foo/bar/baz");
    GURL dest = ArcUrlToExternalFileUrl(src);
    EXPECT_TRUE(dest.is_valid());
    EXPECT_EQ(content::kExternalFileScheme, dest.scheme());
    GURL result = ExternalFileUrlToArcUrl(dest);
    EXPECT_TRUE(result.is_valid());
    EXPECT_EQ(src, result);
  }
  {
    GURL src("content://org.chromium.foo/bar/baz");
    GURL dest = ArcUrlToExternalFileUrl(src);
    EXPECT_TRUE(dest.is_valid());
    EXPECT_EQ(content::kExternalFileScheme, dest.scheme());
    GURL result = ExternalFileUrlToArcUrl(dest);
    EXPECT_TRUE(result.is_valid());
    EXPECT_EQ(src, result);
  }
  {
    GURL src("content://org.chromium.foo/bar/%19%20%21");
    GURL dest = ArcUrlToExternalFileUrl(src);
    EXPECT_TRUE(dest.is_valid());
    EXPECT_EQ(content::kExternalFileScheme, dest.scheme());
    GURL result = ExternalFileUrlToArcUrl(dest);
    EXPECT_TRUE(result.is_valid());
    EXPECT_EQ(src, result);
  }
  {
    GURL src("content://org.chromium.foo/!@#$%^&*()_+|~-=\\`[]{};':\"<>?,./");
    GURL dest = ArcUrlToExternalFileUrl(src);
    EXPECT_TRUE(dest.is_valid());
    EXPECT_EQ(content::kExternalFileScheme, dest.scheme());
    GURL result = ExternalFileUrlToArcUrl(dest);
    EXPECT_TRUE(result.is_valid());
    EXPECT_EQ(src, result);
  }
  {
    base::string16 utf16_string = {
        0x307b,  // HIRAGANA_LETTER_HO
        0x3052,  // HIRAGANA_LETTER_GE
    };
    GURL src("content://org.chromium.foo/" + base::UTF16ToUTF8(utf16_string));
    GURL dest = ArcUrlToExternalFileUrl(src);
    EXPECT_TRUE(dest.is_valid());
    EXPECT_EQ(content::kExternalFileScheme, dest.scheme());
    GURL result = ExternalFileUrlToArcUrl(dest);
    EXPECT_TRUE(result.is_valid());
    EXPECT_EQ(src, result);
  }
}

}  // namespace arc
