// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/net/quoted_printable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class QuotedPrintableTest : public testing::Test {
};

const char* const kNormalText[] = {
  // Basic sentence with an =.
  "If you believe that truth=beauty, then surely mathematics is the most "
  "beautiful branch of philosophy.",

  // All ASCII chars.
  "\x1\x2\x3\x4\x5\x6\x7\x8\x9\xA\xB\xC\xD\xE\xF"
  "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
  "\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C\x2D\x2E\x2F"
  "\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3A\x3B\x3C\x3D\x3E\x3F"
  "\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4A\x4B\x4C\x4D\x4E\x4F"
  "\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5A\x5B\x5C\x5D\x5E\x5F"
  "\x60\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6A\x6B\x6C\x6D\x6E\x6F"
  "\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7A\x7B\x7C\x7D\x7E\x7F"
  "\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8A\x8B\x8C\x8D\x8E\x8F"
  "\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9A\x9B\x9C\x9D\x9E\x9F"
  "\xA0\xA1\xA2\xA3\xA4\xA5\xA6\xA7\xA8\xA9\xAA\xAB\xAC\xAD\xAE\xAF"
  "\xB0\xB1\xB2\xB3\xB4\xB5\xB6\xB7\xB8\xB9\xBA\xBB\xBC\xBD\xBE\xBF"
  "\xC0\xC1\xC2\xC3\xC4\xC5\xC6\xC7\xC8\xC9\xCA\xCB\xCC\xCD\xCE\xCF"
  "\xD0\xD1\xD2\xD3\xD4\xD5\xD6\xD7\xD8\xD9\xDA\xDB\xDC\xDD\xDE\xDF"
  "\xE0\xE1\xE2\xE3\xE4\xE5\xE6\xE7\xE8\xE9\xEA\xEB\xEC\xED\xEE\xEF"
  "\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7\xF8\xF9\xFA\xFB\xFC\xFD\xFE\xFF",

  // Space right before max char per line.
  "This line has a space at the 75 characters mark = ********************** "
  "The end.",

  // Space on max char per line index.
  "This line has a space at the 76 characters mark = *********************** "
  "The end.",

  // Space at end of line.
  "This line ends with a space \r\nThe end.",

  // Space at end of input.
  "This input ends with a space. ",

  // Tab right before max char per line.
  "This line has a tab at the 75 characters mark = ************************\t"
  "The end.",

  // Tab on max char per line index.
  "This line has a tab at the 76 characters mark = *************************\t"
  "The end.",

  // Tab at end of line.
  "This line ends with a tab\t\r\nThe end.",

  // Tab at end of input.
  "This input ends with a tab.\t",

  // Various EOLs in input.
  "This is a test of having EOLs in the input\r\n"
  "Any EOL should be converted \r to \n a CRLF \r\n."
};

const char* const kEncodedText[] = {
  "If you believe that truth=3Dbeauty, then surely mathematics is the most "
  "bea=\r\nutiful branch of philosophy.",

  "=01=02=03=04=05=06=07=08=09\r\n"
  "=0B=0C\r\n"
  "=0E=0F=10=11=12=13=14=15=16=17=18=19=1A=1B=1C=1D=1E=1F !\"#$%&'()*+,-./01234"
    "=\r\n"
  "56789:;<=3D>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}"
    "=\r\n"
  "~=7F=80=81=82=83=84=85=86=87=88=89=8A=8B=8C=8D=8E=8F=90=91=92=93=94=95=96="
    "\r\n"
  "=97=98=99=9A=9B=9C=9D=9E=9F=A0=A1=A2=A3=A4=A5=A6=A7=A8=A9=AA=AB=AC=AD=AE=AF="
    "\r\n"
  "=B0=B1=B2=B3=B4=B5=B6=B7=B8=B9=BA=BB=BC=BD=BE=BF=C0=C1=C2=C3=C4=C5=C6=C7=C8="
    "\r\n"
  "=C9=CA=CB=CC=CD=CE=CF=D0=D1=D2=D3=D4=D5=D6=D7=D8=D9=DA=DB=DC=DD=DE=DF=E0=E1="
    "\r\n"
  "=E2=E3=E4=E5=E6=E7=E8=E9=EA=EB=EC=ED=EE=EF=F0=F1=F2=F3=F4=F5=F6=F7=F8=F9=FA="
    "\r\n"
  "=FB=FC=FD=FE=FF",

  "This line has a space at the 75 characters mark =3D ********************** ="
  "\r\nThe end.",

  "This line has a space at the 76 characters mark =3D ***********************="
  "\r\n The end.",

  "This line ends with a space=20\r\nThe end.",

  "This input ends with a space.=20",

  "This line has a tab at the 75 characters mark =3D ************************\t"
      "=\r\nThe end.",

  "This line has a tab at the 76 characters mark =3D *************************="
  "\r\n\tThe end.",

  "This line ends with a tab=09\r\nThe end.",

  "This input ends with a tab.=09",

  "This is a test of having EOLs in the input\r\n"
  "Any EOL should be converted=20\r\n to=20\r\n a CRLF=20\r\n."
};


const char* const kBadEncodedText[] = {
  // Invalid finish with =.
  "A =3D at the end of the input is bad=",

  // Invalid = sequence.
  "This line contains a valid =3D sequence and invalid ones =$$ = =\t =1 = 2 "
      "==",
};

const char* const kBadEncodedTextDecoded[] = {
  "A = at the end of the input is bad=",

  "This line contains a valid = sequence and invalid ones =$$ = =\t =1 = 2 ==",
};

// Compares the 2 strings and returns true if they are identical, but for EOLs
// that don't have to be the same (ex: \r\n can match \n).
bool CompareEOLInsensitive(const std::string& s1, const std::string& s2) {
  std::string::const_iterator s1_iter = s1.begin();
  std::string::const_iterator s2_iter = s2.begin();

  while (true) {
    if (s1_iter == s1.end() && s2_iter == s2.end())
      return true;
    if ((s1_iter == s1.end() && s2_iter != s2.end()) ||
        (s1_iter != s1.end() && s2_iter == s2.end())) {
      return false;
    }
    int s1_eol = chrome::browser::net::IsEOL(s1_iter, s1);
    int s2_eol = chrome::browser::net::IsEOL(s2_iter, s2);
    if ((!s1_eol && s2_eol) || (s1_eol && !s2_eol)) {
      // Unmatched EOL.
      return false;
    }
    if (s1_eol > 0) {
      s1_iter += s1_eol;
      s2_iter += s2_eol;
    } else {
      // Non-EOL char.
      if (*s1_iter != *s2_iter)
        return false;
      s1_iter++;
      s2_iter++;
    }
  }
  return true;
}

}  // namespace

TEST(QuotedPrintableTest, Encode) {
  ASSERT_EQ(arraysize(kNormalText), arraysize(kEncodedText));
  for (size_t i = 0; i < arraysize(kNormalText); ++i) {
    SCOPED_TRACE(::testing::Message() << "Iteration " << i);
    std::string output;
    chrome::browser::net::QuotedPrintableEncode(kNormalText[i], &output);
    std::string expected(kEncodedText[i]);
    EXPECT_EQ(expected, output);
  }
}

TEST(QuotedPrintableTest, Decode) {
  ASSERT_EQ(arraysize(kNormalText), arraysize(kEncodedText));
  for (size_t i = 0; i < arraysize(kNormalText); ++i) {
    std::string output;
    EXPECT_TRUE(chrome::browser::net::QuotedPrintableDecode(
        kEncodedText[i], &output));
    std::string expected(kNormalText[i]);
    SCOPED_TRACE(::testing::Message() << "Iteration " << i <<
                 "\n  Actual=\n" << output << "\n  Expected=\n" <<
                 expected);
    // We cannot test for equality as EOLs won't match the normal text
    // (as any EOL is converted to a CRLF during encoding).
    EXPECT_TRUE(CompareEOLInsensitive(expected, output));
  }
}

// Tests that we return false but still do our best to decode badly encoded
// inputs.
TEST(QuotedPrintableTest, DecodeBadInput) {
  ASSERT_EQ(arraysize(kBadEncodedText), arraysize(kBadEncodedTextDecoded));
  for (size_t i = 0; i < arraysize(kBadEncodedText); ++i) {
    SCOPED_TRACE(::testing::Message() << "Iteration " << i);
    std::string output;
    EXPECT_FALSE(chrome::browser::net::QuotedPrintableDecode(
        kBadEncodedText[i], &output));
    std::string expected(kBadEncodedTextDecoded[i]);
    EXPECT_EQ(expected, output);
  }
}
