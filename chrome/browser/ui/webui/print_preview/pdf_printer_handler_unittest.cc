// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/pdf_printer_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#define FPL(x) FILE_PATH_LITERAL(x)

using PdfPrinterHandlerTest = testing::Test;

TEST_F(PdfPrinterHandlerTest, GetFileNameForPrintJobTitle) {
  static const struct {
    const char* input;
    const base::FilePath::CharType* expected_output;
  } kTestData[] = {
      {"Foo", FPL("Foo.pdf")},
      {"bar", FPL("bar.pdf")},
      {"qux.html", FPL("qux.html.pdf")},
      {"qux.pdf", FPL("qux.pdf")},
      {"Print Me", FPL("Print Me.pdf")},
      {"Print Me.html", FPL("Print Me.html.pdf")},
      {"1l!egal_F@L#(N)ame.html", FPL("1l!egal_F@L#(N)ame.html.pdf")},
      {"example.com", FPL("example.com.pdf")},
      {"data:text/html,foo", FPL("data_text_html,foo.pdf")},
      {"Baz.com Mail - this is e-mail - what. does it mean",
       FPL("Baz.com Mail - this is e-mail - what. does it mean.pdf")},
      {"Baz.com Mail - this is email - what. does. it. mean?",
       FPL("Baz.com Mail - this is email - what. does. it. mean_.pdf")},
      {"Baz.com Mail - This is email. What does it mean.",
       FPL("Baz.com Mail - This is email. What does it mean_.pdf")},
      {"Baz.com Mail - this is email what does it mean",
       FPL("Baz.com Mail - this is email what does it mean.pdf")},
  };

  for (const auto& data : kTestData) {
    SCOPED_TRACE(data.input);
    base::FilePath path = PdfPrinterHandler::GetFileNameForPrintJobTitle(
        base::ASCIIToUTF16(data.input));
    EXPECT_EQ(data.expected_output, path.value());
  }
}

TEST_F(PdfPrinterHandlerTest, GetFileNameForPrintJobURL) {
  static const struct {
    const char* input;
    const base::FilePath::CharType* expected_output;
  } kTestData[] = {
      {"http://example.com", FPL("example.com.pdf")},
      {"http://example.com/?foo", FPL("example.com.pdf")},
      {"https://example.com/foo.html", FPL("foo.pdf")},
      {"https://example.com/bar/qux.txt", FPL("qux.pdf")},
      {"https://example.com/bar/qux.pdf", FPL("qux.pdf")},
      {"data:text/html,foo", FPL("dataurl.pdf")},
  };

  for (const auto& data : kTestData) {
    SCOPED_TRACE(data.input);
    base::FilePath path =
        PdfPrinterHandler::GetFileNameForURL(GURL(data.input));
    EXPECT_EQ(data.expected_output, path.value());
  }
}
