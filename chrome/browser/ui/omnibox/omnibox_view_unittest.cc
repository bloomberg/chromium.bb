// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace {

class OmniboxViewTest : public PlatformTest {
 public:
  virtual void TearDown() OVERRIDE {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

 private:
  // Windows requires a message loop for clipboard access.
  base::MessageLoopForUI message_loop_;
};

TEST_F(OmniboxViewTest, TestStripSchemasUnsafeForPaste) {
  const char* urls[] = {
    "http://www.google.com?q=javascript:alert(0)",  // Safe URL.
    "javAscript:alert(0)",                          // Unsafe JS URL.
    "jaVascript:\njavaScript: alert(0)"             // Single strip unsafe.
  };

  const char* expecteds[] = {
    "http://www.google.com?q=javascript:alert(0)",  // Safe URL.
    "alert(0)",                                     // Unsafe JS URL.
    "alert(0)"                                      // Single strip unsafe.
  };

  for (size_t i = 0; i < arraysize(urls); i++) {
    EXPECT_EQ(ASCIIToUTF16(expecteds[i]),
              OmniboxView::StripJavascriptSchemas(ASCIIToUTF16(urls[i])));
  }
}

TEST_F(OmniboxViewTest, SanitizeTextForPaste) {
  // Broken URL has newlines stripped.
  const string16 kWrappedURL(ASCIIToUTF16(
      "http://www.chromium.org/developers/testing/chromium-\n"
      "build-infrastructure/tour-of-the-chromium-buildbot"));

  const string16 kFixedURL(ASCIIToUTF16(
      "http://www.chromium.org/developers/testing/chromium-"
      "build-infrastructure/tour-of-the-chromium-buildbot"));
  EXPECT_EQ(kFixedURL, OmniboxView::SanitizeTextForPaste(kWrappedURL));

  // Multi-line address is converted to a single-line address.
  const string16 kWrappedAddress(ASCIIToUTF16(
      "1600 Amphitheatre Parkway\nMountain View, CA"));

  const string16 kFixedAddress(ASCIIToUTF16(
      "1600 Amphitheatre Parkway Mountain View, CA"));
  EXPECT_EQ(kFixedAddress, OmniboxView::SanitizeTextForPaste(kWrappedAddress));
}

TEST_F(OmniboxViewTest, GetClipboardText) {
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();

  const string16 kPlainText(ASCIIToUTF16("test text"));
  const std::string kURL("http://www.example.com/");

  // Can we pull straight text off the clipboard?
  {
    ui::ScopedClipboardWriter clipboard_writer(clipboard,
                                               ui::CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteText(kPlainText);
  }
  EXPECT_EQ(kPlainText, OmniboxView::GetClipboardText());

  // Can we pull a string consists of white-space?
  const string16 kSpace6(ASCIIToUTF16("      "));
  const string16 kSpace1(ASCIIToUTF16(" "));
  {
    ui::ScopedClipboardWriter clipboard_writer(clipboard,
                                               ui::CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteText(kSpace6);
  }
  EXPECT_EQ(kSpace1, OmniboxView::GetClipboardText());

  // TODO(shess): ChromeOS hits a DCHECK() at CommitToClipboard() if
  // ObjectMap is empty.  http://crbug.com/133848
#if !defined(OS_CHROMEOS)
  // Does an empty clipboard get empty text?
  clipboard->WriteObjects(ui::CLIPBOARD_TYPE_COPY_PASTE,
                          ui::Clipboard::ObjectMap());
  EXPECT_EQ(string16(), OmniboxView::GetClipboardText());
#endif

  // Bookmark clipboard apparently not supported on Linux.
  // See TODO on ClipboardText.BookmarkTest.
#if !defined(OS_POSIX) || defined(OS_MACOSX)
  const string16 kTitle(ASCIIToUTF16("The Example Company"));
  // Can we pull a bookmark off the clipboard?
  {
    ui::ScopedClipboardWriter clipboard_writer(clipboard,
                                               ui::CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteBookmark(kTitle, kURL);
  }
  EXPECT_EQ(ASCIIToUTF16(kURL), OmniboxView::GetClipboardText());

  // Do we pull text in preference to a bookmark?
  {
    ui::ScopedClipboardWriter clipboard_writer(clipboard,
                                               ui::CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteText(kPlainText);
    clipboard_writer.WriteBookmark(kTitle, kURL);
  }
  EXPECT_EQ(kPlainText, OmniboxView::GetClipboardText());
#endif

  // Do we get nothing if there is neither text nor a bookmark?
  {
    const string16 kMarkup(ASCIIToUTF16("<strong>Hi!</string>"));
    ui::ScopedClipboardWriter clipboard_writer(clipboard,
                                               ui::CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteHTML(kMarkup, kURL);
  }
  EXPECT_TRUE(OmniboxView::GetClipboardText().empty());
}

}  // namespace
