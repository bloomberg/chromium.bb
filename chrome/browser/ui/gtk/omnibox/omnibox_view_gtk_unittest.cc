// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/omnibox/omnibox_view_gtk.h"

#include <gtk/gtk.h>

#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/gfx/image/image.h"

namespace {
class OmniboxEditControllerMock : public OmniboxEditController {
 public:
  MOCK_METHOD4(OnAutocompleteAccept, void(const GURL& url,
                                          WindowOpenDisposition disposition,
                                          content::PageTransition transition,
                                          const GURL& alternate_nav_url));
  MOCK_METHOD0(OnChanged, void());
  MOCK_METHOD0(OnSelectionBoundsChanged, void());
  MOCK_METHOD1(OnInputInProgress, void(bool in_progress));
  MOCK_METHOD0(OnKillFocus, void());
  MOCK_METHOD0(OnSetFocus, void());
  MOCK_CONST_METHOD0(GetFavicon, gfx::Image());
  MOCK_CONST_METHOD0(GetTitle, string16());
  MOCK_METHOD0(GetInstant, InstantController*());
  MOCK_CONST_METHOD0(GetWebContents, content::WebContents*());

  virtual ~OmniboxEditControllerMock() {}
};
}  // namespace

class OmniboxViewGtkTest : public PlatformTest {
 public:
  OmniboxViewGtkTest() : file_thread_(content::BrowserThread::UI) {}

  virtual void SetUp() {
    PlatformTest::SetUp();
    profile_.reset(new TestingProfile);
    window_ = gtk_window_new(GTK_WINDOW_POPUP);
    controller_.reset(new OmniboxEditControllerMock);
    view_.reset(new OmniboxViewGtk(controller_.get(), NULL,
                                   NULL,
                                   profile_.get(),
                                   NULL, false, window_));
    view_->Init();
    text_buffer_ = view_->text_buffer_;
  }

  virtual void TearDown() {
    gtk_widget_destroy(window_);
    PlatformTest::TearDown();
  }

  void SetPasteClipboardRequested(bool b) {
    view_->paste_clipboard_requested_ = b;
  }

  scoped_ptr<OmniboxEditControllerMock> controller_;
  scoped_ptr<TestingProfile> profile_;
  GtkTextBuffer* text_buffer_;
  scoped_ptr<OmniboxViewGtk> view_;
  GtkWidget* window_;
  content::TestBrowserThread file_thread_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OmniboxViewGtkTest);
};

TEST_F(OmniboxViewGtkTest, InsertText) {
  GtkTextIter i;
  const char input[] = "  hello, world ";
  const std::string expected = input;
  gtk_text_buffer_get_iter_at_offset(text_buffer_, &i, 0);
  gtk_text_buffer_insert(text_buffer_, &i, input, strlen(input));
  ASSERT_EQ(expected, UTF16ToUTF8(view_->GetText()));
}

TEST_F(OmniboxViewGtkTest, PasteText) {
  GtkTextIter i;
  const char input[] = "  hello, world ";
  const std::string expected = "hello, world";
  SetPasteClipboardRequested(true);
  view_->OnBeforePossibleChange();
  gtk_text_buffer_get_iter_at_offset(text_buffer_, &i, 0);
  gtk_text_buffer_insert(text_buffer_, &i, input, strlen(input));

  ASSERT_EQ(expected, UTF16ToUTF8(view_->GetText()));
}
