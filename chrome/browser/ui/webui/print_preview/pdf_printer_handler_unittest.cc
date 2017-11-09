// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/pdf_printer_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

using PdfPrinterHandlerTest = testing::Test;

TEST_F(PdfPrinterHandlerTest, GetFileNameForPrintJobTitle) {
  static const struct {
    const char* input;
    const base::FilePath::CharType* expected_output;
  } kTestData[] = {
      {"Foo", FILE_PATH_LITERAL("Foo.pdf")},
      {"bar", FILE_PATH_LITERAL("bar.pdf")},
      {"qux.html", FILE_PATH_LITERAL("qux.pdf")},
      {"Print Me", FILE_PATH_LITERAL("Print Me.pdf")},
      {"Print Me.html", FILE_PATH_LITERAL("Print Me.pdf")},
      {"1l!egal_F@L#(N)ame.html", FILE_PATH_LITERAL("1l!egal_F@L#(N)ame.pdf")},
      // TODO(thestig): Fix for https://crbug.com/782041
      // Should be "example.com.pdf", like the regular file save dialog.
      {"example.com", FILE_PATH_LITERAL("example.pdf")},
  };

  for (const auto& data : kTestData) {
    SCOPED_TRACE(data.input);
    base::FilePath path = PdfPrinterHandler::GetFileNameForPrintJobTitle(
        base::ASCIIToUTF16(data.input));
    EXPECT_EQ(data.expected_output, path.value());
  }
}
