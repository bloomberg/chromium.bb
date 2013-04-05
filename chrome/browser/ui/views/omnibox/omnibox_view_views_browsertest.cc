// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/omnibox/omnibox_views.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "grit/generated_resources.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/views/controls/textfield/native_textfield_wrapper.h"

typedef InProcessBrowserTest OmniboxViewViewsTest;

IN_PROC_BROWSER_TEST_F(OmniboxViewViewsTest, PasteAndGoDoesNotLeavePopupOpen) {
  OmniboxView* view = browser()->window()->GetLocationBar()->GetLocationEntry();
  OmniboxViewViews* omnibox_view_views = GetOmniboxViewViews(view);
  // This test is only relevant when OmniboxViewViews is present and is using
  // the native textfield wrapper.
  if (!omnibox_view_views)
    return;
  views::NativeTextfieldWrapper* native_textfield_wrapper =
      static_cast<views::NativeTextfieldWrapper*>(
          omnibox_view_views->GetNativeWrapperForTesting());
  if (!native_textfield_wrapper)
    return;

  // Put an URL on the clipboard.
  {
    ui::ScopedClipboardWriter clipboard_writer(
        ui::Clipboard::GetForCurrentThread(), ui::Clipboard::BUFFER_STANDARD);
    clipboard_writer.WriteURL(ASCIIToUTF16("http://www.example.com/"));
  }

  // Paste and go.
  native_textfield_wrapper->ExecuteTextCommand(IDS_PASTE_AND_GO);

  // The popup should not be open.
  EXPECT_FALSE(view->model()->popup_model()->IsOpen());
}
