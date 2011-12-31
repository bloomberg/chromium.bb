// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/test_html_dialog_ui_delegate.h"

#include "base/utf_string_conversions.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace test {

TestHtmlDialogUIDelegate::TestHtmlDialogUIDelegate(const GURL& url)
    : url_(url),
      size_(400, 400) {
}

TestHtmlDialogUIDelegate::~TestHtmlDialogUIDelegate() {
}

bool TestHtmlDialogUIDelegate::IsDialogModal() const {
  return true;
}

string16 TestHtmlDialogUIDelegate::GetDialogTitle() const {
  return UTF8ToUTF16("Test");
}

GURL TestHtmlDialogUIDelegate::GetDialogContentURL() const {
  return url_;
}

void TestHtmlDialogUIDelegate::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
}

void TestHtmlDialogUIDelegate::GetDialogSize(gfx::Size* size) const {
  *size = size_;
}

std::string TestHtmlDialogUIDelegate::GetDialogArgs() const {
  return std::string();
}

void TestHtmlDialogUIDelegate::OnDialogClosed(const std::string& json_retval) {
}

void TestHtmlDialogUIDelegate::OnCloseContents(WebContents* source,
    bool* out_close_dialog) {
  if (out_close_dialog)
    *out_close_dialog = true;
}

bool TestHtmlDialogUIDelegate::ShouldShowDialogTitle() const {
  return true;
}

}  // namespace test
