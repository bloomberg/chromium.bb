// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/textfield_views.h"

#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/dom_ui/textfields_ui.h"
#include "chrome/browser/tab_contents/tab_contents.h"

TextfieldViews::TextfieldViews() : DOMView() {}

std::wstring TextfieldViews::GetText() {
  TextfieldsUI* textfields_ui = dom_ui();
  return (textfields_ui) ? textfields_ui->text() : std::wstring();
}

void TextfieldViews::SetText(const std::wstring& text) {
  TextfieldsUI* textfields_ui = dom_ui();
  if (textfields_ui) {
    StringValue text_value(WideToUTF16(text));
    textfields_ui->CallJavascriptFunction(L"setTextfieldValue", text_value);
  }
  SchedulePaint();
}

TextfieldsUI* TextfieldViews::dom_ui() {
  TextfieldsUI* dom_ui = NULL;
  if (tab_contents_.get() && tab_contents_->dom_ui()) {
    dom_ui = static_cast<TextfieldsUI*>(tab_contents_->dom_ui());
  }
  return dom_ui;
}
