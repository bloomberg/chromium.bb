// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/pdf/pdf_browsertest_base.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/clipboard/clipboard.h"

// Note: All tests in here require the internal PDF plugin, so they're disabled
// in non-official builds. We still compile them though, to prevent bitrot.
//
// These tests are interactive because they access the clipboard.

namespace {

const char kSearchKeyword[] = "adipiscing";

#if defined(GOOGLE_CHROME_BUILD) && (defined(OS_WIN) || defined(OS_LINUX))
#define MAYBE_FindAndCopy FindAndCopy
#else
// TODO(thestig): http://crbug.com/79837, http://crbug.com/329912
#define MAYBE_FindAndCopy DISABLED_FindAndCopy
#endif
IN_PROC_BROWSER_TEST_F(PDFBrowserTest, MAYBE_FindAndCopy) {
  ASSERT_NO_FATAL_FAILURE(Load());
  // Verifies that find in page works.
  ASSERT_EQ(3, ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(),
      base::ASCIIToUTF16(kSearchKeyword),
      true, false, NULL, NULL));

  // Verify that copying selected text works.
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  // Reset the clipboard first.
  clipboard->Clear(ui::CLIPBOARD_TYPE_COPY_PASTE);

  browser()->tab_strip_model()->GetActiveWebContents()->Copy();
  ASSERT_NO_FATAL_FAILURE(WaitForResponse());

  std::string text;
  clipboard->ReadAsciiText(ui::CLIPBOARD_TYPE_COPY_PASTE, &text);
  EXPECT_EQ(kSearchKeyword, text);
}

}  // namespace
