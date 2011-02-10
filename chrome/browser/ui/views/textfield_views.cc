// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/textfield_views.h"

#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/dom_ui/textfields_ui.h"
#include "chrome/browser/tab_contents/tab_contents.h"

TextfieldViews::TextfieldViews() : DOMView() {}

std::wstring TextfieldViews::GetText() {
  TextfieldsUI* textfields_ui = web_ui();
  return (textfields_ui) ? textfields_ui->text() : std::wstring();
}

void TextfieldViews::SetText(const std::wstring& text) {
  TextfieldsUI* textfields_ui = web_ui();
  if (textfields_ui) {
    StringValue text_value(WideToUTF16(text));
    textfields_ui->CallJavascriptFunction(L"setTextfieldValue", text_value);
  }
  SchedulePaint();
}

TextfieldsUI* TextfieldViews::web_ui() {
  TextfieldsUI* web_ui = NULL;
  if (tab_contents_.get() && tab_contents_->web_ui()) {
    web_ui = static_cast<TextfieldsUI*>(tab_contents_->web_ui());
  }
  return web_ui;
}
