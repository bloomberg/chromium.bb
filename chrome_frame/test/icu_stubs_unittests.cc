// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/chrome_frame_unittests.h"

// Need to include these first since they're included
#include "base/logging.h"
#include "base/string_util.h"
#include "googleurl/src/url_canon.h"

// Include the implementation of our stubs into a special namespace so that
// we can separate them from Chrome's implementation.
namespace icu_stubs {
  // This struct is only to avoid build problems for the two googleurl stubs
  // that currently are noops.
  struct CanonOutputW { };

  #include "chrome_frame/icu_stubs.cc"
}  // namespace icu_stubs

// anonymous namespace for test data.
namespace {

  // Test strings borrowed from base/string_util_unittest.cc
  static const wchar_t* const kConvertRoundtripCases[] = {
    L"",
    L"Google Vid¯ôfY »"
    L"\x7f51\x9875\x0020\x56fe\x7247\x0020\x8d44\x8baf\x66f4\x591a\x0020\x00bb",
    //  " ±³ºÌÃ¼¹¿Â ÃÄÌÂ"
    L"\x03a0\x03b1\x03b3\x03ba\x03cc\x03c3\x03bc\x03b9"
    L"\x03bf\x03c2\x0020\x0399\x03c3\x03c4\x03cc\x03c2",
    // ">8A: AB@0=8F =0 @CAA:><"
    L"\x041f\x043e\x0438\x0441\x043a\x0020\x0441\x0442"
    L"\x0440\x0430\x043d\x0438\x0446\x0020\x043d\x0430"
    L"\x0020\x0440\x0443\x0441\x0441\x043a\x043e\x043c",
    // "È´ÌÁD¾¤Â"
    L"\xc804\xccb4\xc11c\xbe44\xc2a4",

    // Test characters that take more than 16 bits. This will depend on whether
    // wchar_t is 16 or 32 bits.
    #if defined(WCHAR_T_IS_UTF16)
    L"\xd800\xdf00",
    // ?????  (Mathematical Alphanumeric Symbols (U+011d40 - U+011d44 : A,B,C,D,E)
    L"\xd807\xdd40\xd807\xdd41\xd807\xdd42\xd807\xdd43\xd807\xdd44",
    #elif defined(WCHAR_T_IS_UTF32)
    L"\x10300",
    // ?????  (Mathematical Alphanumeric Symbols (U+011d40 - U+011d44 : A,B,C,D,E)
    L"\x11d40\x11d41\x11d42\x11d43\x11d44",
    #endif
  };

}  // namespace

TEST(IcuStubsTests, UTF8AndWideStubTest) {
  // Test code borrowed from ConvertUTF8AndWide in base/string_util_unittest.cc.

  // The difference is that we want to make sure that our stubs work the same
  // way as chrome's implementation of WideToUTF8 and UTF8ToWide.
  for (size_t i = 0; i < arraysize(kConvertRoundtripCases); ++i) {
    std::ostringstream utf8_base, utf8_stub;
    utf8_base << WideToUTF8(kConvertRoundtripCases[i]);
    utf8_stub << icu_stubs::WideToUTF8(kConvertRoundtripCases[i]);

    EXPECT_EQ(utf8_base.str(), utf8_stub.str());

    std::wostringstream wide_base, wide_stub;
    wide_base << UTF8ToWide(utf8_base.str());
    wide_stub << icu_stubs::UTF8ToWide(utf8_base.str());

    EXPECT_EQ(wide_base.str(), wide_stub.str());
  }
}
