// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/fake_desktop_media_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/browser/desktop_media_id.h"

class DesktopMediaPickerViewsBrowserTest : public DialogBrowserTest {
 public:
  DesktopMediaPickerViewsBrowserTest() {}

  // DialogBrowserTest:
  void ShowDialog(const std::string& name) override {
    picker_ = base::MakeUnique<DesktopMediaPickerViews>();
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    gfx::NativeWindow native_window = browser()->window()->GetNativeWindow();

    std::vector<std::unique_ptr<DesktopMediaList>> source_lists;
    for (auto type : {content::DesktopMediaID::SOURCE_SCREEN,
                      content::DesktopMediaID::SOURCE_WINDOW,
                      content::DesktopMediaID::SOURCE_WEB_CONTENTS}) {
      source_lists.push_back(base::MakeUnique<FakeDesktopMediaList>(type));
    }

    picker_->Show(web_contents, native_window, nullptr,
                  base::ASCIIToUTF16("app_name"),
                  base::ASCIIToUTF16("target_name"), std::move(source_lists),
                  true, DesktopMediaPicker::DoneCallback());
  }

 private:
  std::unique_ptr<DesktopMediaPickerViews> picker_;

  DISALLOW_COPY_AND_ASSIGN(DesktopMediaPickerViewsBrowserTest);
};

// Invokes a dialog that allows the user to select what view of their desktop
// they would like to share. See test_browser_dialog.h.
IN_PROC_BROWSER_TEST_F(DesktopMediaPickerViewsBrowserTest,
                       InvokeDialog_default) {
  RunDialog();
}
