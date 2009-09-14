// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/base/string.h"

namespace notifier {

TEST_NOTIFIER_F(StringTest);

TEST_F(StringTest, StringToInt) {
  ASSERT_EQ(StringToInt("625"), 625);
  ASSERT_EQ(StringToInt("6"), 6);
  ASSERT_EQ(StringToInt("0"), 0);
  ASSERT_EQ(StringToInt(" 122"), 122);
  ASSERT_EQ(StringToInt("a"), 0);
  ASSERT_EQ(StringToInt(" a"), 0);
  ASSERT_EQ(StringToInt("2147483647"), 2147483647);
  ASSERT_EQ(StringToInt("-2147483648"),
            static_cast<int>(0x80000000));  // Hex constant avoids gcc warning.

  int value = 0;
  ASSERT_FALSE(ParseStringToInt("62.5", &value, true));
  ASSERT_FALSE(ParseStringToInt("625e", &value, true));
  ASSERT_FALSE(ParseStringToInt("2147483648", &value, true));
  ASSERT_FALSE(ParseStringToInt("-2147483649", &value, true));
  ASSERT_FALSE(ParseStringToInt("-4857004031", &value, true));
}

TEST_F(StringTest, StringToUint) {
  ASSERT_EQ(StringToUint("625"), 625);
  ASSERT_EQ(StringToUint("6"), 6);
  ASSERT_EQ(StringToUint("0"), 0);
  ASSERT_EQ(StringToUint(" 122"), 122);
  ASSERT_EQ(StringToUint("a"), 0);
  ASSERT_EQ(StringToUint(" a"), 0);
  ASSERT_EQ(StringToUint("4294967295"), static_cast<uint32>(0xffffffff));

  uint32 value = 0;
  ASSERT_FALSE(ParseStringToUint("62.5", &value, true));
  ASSERT_FALSE(ParseStringToUint("625e", &value, true));
  ASSERT_FALSE(ParseStringToUint("4294967296", &value, true));
  ASSERT_FALSE(ParseStringToUint("-1", &value, true));
}

TEST_F(StringTest, StringToInt64) {
  ASSERT_EQ(StringToInt64("119600064000000000"),
            INT64_C(119600064000000000));
  ASSERT_EQ(StringToInt64(" 119600064000000000"),
            INT64_C(119600064000000000));
  ASSERT_EQ(StringToInt64("625"), 625);
  ASSERT_EQ(StringToInt64("6"), 6);
  ASSERT_EQ(StringToInt64("0"), 0);
  ASSERT_EQ(StringToInt64(" 122"), 122);
  ASSERT_EQ(StringToInt64("a"), 0);
  ASSERT_EQ(StringToInt64(" a"), 0);
  ASSERT_EQ(StringToInt64("9223372036854775807"), INT64_C(9223372036854775807));
  ASSERT_EQ(StringToInt64("-9223372036854775808I64"),
            static_cast<int64>(INT64_C(0x8000000000000000)));

  int64 value = 0;
  ASSERT_FALSE(ParseStringToInt64("62.5", &value, true));
  ASSERT_FALSE(ParseStringToInt64("625e", &value, true));
  ASSERT_FALSE(ParseStringToInt64("9223372036854775808", &value, true));
  ASSERT_FALSE(ParseStringToInt64("-9223372036854775809", &value, true));
}

TEST_F(StringTest, StringToDouble) {
  ASSERT_DOUBLE_EQ(StringToDouble("625"), 625);
  ASSERT_DOUBLE_EQ(StringToDouble("-625"), -625);
  ASSERT_DOUBLE_EQ(StringToDouble("-6.25"), -6.25);
  ASSERT_DOUBLE_EQ(StringToDouble("6.25"), 6.25);
  ASSERT_DOUBLE_EQ(StringToDouble("0.00"), 0);
  ASSERT_DOUBLE_EQ(StringToDouble(" 55.1"), 55.1);
  ASSERT_DOUBLE_EQ(StringToDouble(" 55.001"), 55.001);
  ASSERT_DOUBLE_EQ(StringToDouble("  1.001"), 1.001);

  double value = 0.0;
  ASSERT_FALSE(ParseStringToDouble("62*5", &value, true));
}

TEST_F(StringTest, Int64ToHexString) {
  ASSERT_STREQ("1a8e79fe1d58000",
               Int64ToHexString(INT64_C(119600064000000000)).c_str());
  ASSERT_STREQ("271", Int64ToHexString(625).c_str());
  ASSERT_STREQ("0", Int64ToHexString(0).c_str());
}

TEST_F(StringTest, StringStartsWith) {
  { std::string s(""); ASSERT_TRUE(StringStartsWith(s, "")); }
  { std::string s("abc"); ASSERT_TRUE(StringStartsWith(s, "ab")); }
  { std::string s("abc"); ASSERT_FALSE(StringStartsWith(s, "bc")); }
}

TEST_F(StringTest, StringEndsWith) {
  { std::string s(""); ASSERT_TRUE(StringEndsWith(s, "")); }
  { std::string s("abc"); ASSERT_TRUE(StringEndsWith(s, "bc")); }
  { std::string s("abc"); ASSERT_FALSE(StringEndsWith(s, "ab")); }
}

TEST_F(StringTest, MakeStringEndWith) {
  {
    std::string s("");
    std::string t(MakeStringEndWith(s, ""));
    ASSERT_STREQ(t.c_str(), "");
  }
  {
    std::string s("abc");
    std::string t(MakeStringEndWith(s, "def"));
    ASSERT_STREQ(t.c_str(), "abcdef");
  }
  {
    std::string s("abc");
    std::string t(MakeStringEndWith(s, "bc"));
    ASSERT_STREQ(t.c_str(), "abc");
  }
}

TEST_F(StringTest, LowerString) {
  { std::string s("");  LowerString(&s);  ASSERT_STREQ(s.c_str(), ""); }
  { std::string s("a");  LowerString(&s);  ASSERT_STREQ(s.c_str(), "a"); }
  { std::string s("A");  LowerString(&s);  ASSERT_STREQ(s.c_str(), "a"); }
  { std::string s("abc"); LowerString(&s); ASSERT_STREQ(s.c_str(), "abc"); }
  { std::string s("ABC"); LowerString(&s); ASSERT_STREQ(s.c_str(), "abc"); }
}

TEST_F(StringTest, UpperString) {
  { std::string s("");  UpperString(&s);  ASSERT_STREQ(s.c_str(), ""); }
  { std::string s("A");  UpperString(&s);  ASSERT_STREQ(s.c_str(), "A"); }
  { std::string s("a");  UpperString(&s);  ASSERT_STREQ(s.c_str(), "A"); }
  { std::string s("ABC"); UpperString(&s); ASSERT_STREQ(s.c_str(), "ABC"); }
  { std::string s("abc"); UpperString(&s); ASSERT_STREQ(s.c_str(), "ABC"); }
}

TEST_F(StringTest, TrimString) {
  const char* white = " \n\t";
  std::string s, c;

  // TrimStringLeft.
  s = "";  // empty
  c = "";
  ASSERT_EQ(TrimStringLeft(&s, white), 0);
  ASSERT_STREQ(s.c_str(), c.c_str());

  s = " \n\t";  // all bad
  c = "";
  ASSERT_EQ(TrimStringLeft(&s, white), 3);
  ASSERT_STREQ(s.c_str(), c.c_str());

  s = "dog";  // nothing bad
  c = "dog";
  ASSERT_EQ(TrimStringLeft(&s, white), 0);
  ASSERT_STREQ(s.c_str(), c.c_str());

  s = " dog ";  // some bad
  c = "dog ";
  ASSERT_EQ(TrimStringLeft(&s, white), 1);
  ASSERT_STREQ(s.c_str(), c.c_str());

  s = " \n\t\t I love my little dog \n\t ";
  c = "I love my little dog \n\t ";
  ASSERT_EQ(TrimStringLeft(&s, white), 5);
  ASSERT_STREQ(s.c_str(), c.c_str());

  // TrimStringRight.
  s = "";
  c = "";
  ASSERT_EQ(TrimStringRight(&s, white), 0);
  ASSERT_STREQ(s.c_str(), c.c_str());

  s = " \n\t";
  c = "";
  ASSERT_EQ(TrimStringRight(&s, white), 3);
  ASSERT_STREQ(s.c_str(), c.c_str());

  s = "dog";
  c = "dog";
  ASSERT_EQ(TrimStringRight(&s, white), 0);
  ASSERT_STREQ(s.c_str(), c.c_str());

  s = " dog ";
  c = " dog";
  ASSERT_EQ(TrimStringRight(&s, white), 1);
  ASSERT_STREQ(s.c_str(), c.c_str());

  s = " \n\t\t I love my little dog \n\t ";
  c = " \n\t\t I love my little dog";
  ASSERT_EQ(TrimStringRight(&s, white), 4);
  ASSERT_STREQ(s.c_str(), c.c_str());

  // TrimString.
  s = "";
  c = "";
  ASSERT_EQ(TrimString(&s, white), 0);
  ASSERT_STREQ(s.c_str(), c.c_str());

  s = " \n\t";
  c = "";
  ASSERT_EQ(TrimString(&s, white), 3);
  ASSERT_STREQ(s.c_str(), c.c_str());

  s = "dog";
  c = "dog";
  ASSERT_EQ(TrimString(&s, white), 0);
  ASSERT_STREQ(s.c_str(), c.c_str());

  s = " dog ";
  c = "dog";
  ASSERT_EQ(TrimString(&s, white), 2);
  ASSERT_STREQ(s.c_str(), c.c_str());

  s = " \n\t\t I love my little dog \n\t ";
  c = "I love my little dog";
  ASSERT_EQ(TrimString(&s, white), 9);
  ASSERT_STREQ(s.c_str(), c.c_str());
}

TEST_F(StringTest, SplitOneStringToken) {
  const char* teststrings[] = {
    "alongword",
    "alongword ",
    "alongword  ",
    "alongword anotherword",
    " alongword",
    "",
  };
  const char* source = NULL;

  source = teststrings[0];
  ASSERT_STREQ(SplitOneStringToken(&source, " ").c_str(), "alongword");
  ASSERT_STREQ(source, NULL);

  source = teststrings[1];
  ASSERT_STREQ(SplitOneStringToken(&source, " ").c_str(), "alongword");
  ASSERT_STREQ(source, teststrings[1] + strlen("alongword") + 1);

  source = teststrings[2];
  ASSERT_STREQ(SplitOneStringToken(&source, " ").c_str(), "alongword");
  ASSERT_STREQ(source, teststrings[2] + strlen("alongword") + 1);

  source = teststrings[3];
  ASSERT_STREQ(SplitOneStringToken(&source, " ").c_str(), "alongword");
  ASSERT_STREQ(source, teststrings[3] + strlen("alongword") + 1);

  source = teststrings[4];
  ASSERT_STREQ(SplitOneStringToken(&source, " ").c_str(), "");
  ASSERT_STREQ(source, teststrings[4] + 1);

  source = teststrings[5];
  ASSERT_STREQ(SplitOneStringToken(&source, " ").c_str(), "");
  ASSERT_STREQ(source, NULL);
}

TEST_F(StringTest, FixedString) {
  // Test basic operation.
  const wchar_t kData[] = L"hello world";
  FixedString<wchar_t, 40> buf;

  buf.Append(kData);
  EXPECT_EQ(arraysize(kData)-1, buf.size());
  EXPECT_EQ(0, wcscmp(kData, buf.get()));

  buf.Append(' ');
  buf.Append(kData);
  const wchar_t kExpected[] = L"hello world hello world";
  EXPECT_EQ(arraysize(kExpected)-1, buf.size());
  EXPECT_EQ(0, wcscmp(kExpected, buf.get()));
  EXPECT_EQ(false, buf.was_truncated());

  // Test overflow.
  FixedString<wchar_t, 5> buf2;
  buf2.Append(L"hello world");
  EXPECT_EQ(static_cast<size_t>(0), buf2.size());
  EXPECT_EQ(0, buf2.get()[0]);
  EXPECT_EQ(true, buf2.was_truncated());
}

TEST_F(StringTest, LowerToPascalCase) {
  EXPECT_STREQ("", LowerWithUnderToPascalCase("").c_str());
  EXPECT_STREQ("A", LowerWithUnderToPascalCase("a").c_str());
  EXPECT_STREQ("TestS", LowerWithUnderToPascalCase("test_s").c_str());
  EXPECT_STREQ("XQ", LowerWithUnderToPascalCase("x_q").c_str());
  EXPECT_STREQ("XQDNS", LowerWithUnderToPascalCase("x_qDNS").c_str());
}

TEST_F(StringTest, PascalCaseToLower) {
  EXPECT_STREQ("", PascalCaseToLowerWithUnder("").c_str());
  EXPECT_STREQ("a", PascalCaseToLowerWithUnder("A").c_str());
  EXPECT_STREQ("test_s", PascalCaseToLowerWithUnder("TestS").c_str());
  EXPECT_STREQ("xq", PascalCaseToLowerWithUnder("XQ").c_str());
  EXPECT_STREQ("dns_name", PascalCaseToLowerWithUnder("DNSName").c_str());
  EXPECT_STREQ("xqdns", PascalCaseToLowerWithUnder("XQDNS").c_str());
  EXPECT_STREQ("xqdn_sa", PascalCaseToLowerWithUnder("XQDNSa").c_str());
  EXPECT_STREQ("dns1", PascalCaseToLowerWithUnder("DNS1").c_str());
}

TEST_F(StringTest, HtmlEncode) {
  EXPECT_STREQ("dns", HtmlEncode("dns").c_str());
  EXPECT_STREQ("&amp;", HtmlEncode("&").c_str());
  EXPECT_STREQ("&amp;amp;", HtmlEncode("&amp;").c_str());
  EXPECT_STREQ("&lt;!&gt;", HtmlEncode("<!>").c_str());
}

TEST_F(StringTest, HtmlDecode) {
  EXPECT_STREQ("dns", HtmlDecode("dns").c_str());
  EXPECT_STREQ("&", HtmlDecode("&amp;").c_str());
  EXPECT_STREQ("&amp;", HtmlDecode("&amp;amp;").c_str());
  EXPECT_STREQ("<!>", HtmlDecode("&lt;!&gt;").c_str());
}

TEST_F(StringTest, UrlEncode) {
  EXPECT_STREQ("%26", UrlEncode("&").c_str());
  EXPECT_STREQ("%3f%20", UrlEncode("? ").c_str());
  EXPECT_STREQ("as%20dfdsa", UrlEncode("as dfdsa").c_str());
  EXPECT_STREQ("%3c!%3e", UrlEncode("<!>").c_str());
  EXPECT_STREQ("!%23!", UrlEncode("!#!").c_str());
  EXPECT_STREQ("!!", UrlEncode("!!").c_str());
}

TEST_F(StringTest, UrlDecode) {
  EXPECT_STREQ("&", UrlDecode("%26").c_str());
  EXPECT_STREQ("? ", UrlDecode("%3f%20").c_str());
  EXPECT_STREQ("as dfdsa", UrlDecode("as%20dfdsa").c_str());
  EXPECT_STREQ("<!>", UrlDecode("%3c!%3e").c_str());
  EXPECT_STREQ("&amp;", UrlDecode("&amp;").c_str());
}

TEST_F(StringTest, StringReplace) {
  // Test StringReplace core functionality.
  std::string s = "<attribute name=abcd/>";
  StringReplace(&s, "=", " = ", false);
  EXPECT_STREQ(s.c_str(), "<attribute name = abcd/>");

  // Test for negative case.
  s = "<attribute name=abcd/>";
  StringReplace(&s, "-", "=", false);
  EXPECT_STREQ(s.c_str(), "<attribute name=abcd/>");

  // Test StringReplace core functionality with replace_all flag set.
  s = "<attribute name==abcd/>";
  StringReplace(&s, "=", " = ", true);
  EXPECT_STREQ(s.c_str(), "<attribute name =  = abcd/>");

  // Input is an empty string.
  s = "";
  StringReplace(&s, "=", " = ", false);
  EXPECT_STREQ(s.c_str(), "");

  // Input is an empty string and this is a request for repeated string
  // replaces.
  s = "";
  StringReplace(&s, "=", " = ", true);
  EXPECT_STREQ(s.c_str(), "");

  // Input and string to replace is an empty string.
  s = "";
  StringReplace(&s, "", " = ", false);
  EXPECT_STREQ(s.c_str(), "");
}

}  // namespace notifier
