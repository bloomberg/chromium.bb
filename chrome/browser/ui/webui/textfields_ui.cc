// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/textfields_ui.h"

#include <algorithm>
#include <string>

#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

/**
 * TextfieldsUIHTMLSource implementation.
 */
TextfieldsUIHTMLSource::TextfieldsUIHTMLSource()
    : DataSource(chrome::kChromeUITextfieldsHost, MessageLoop::current()) {
}

void TextfieldsUIHTMLSource::StartDataRequest(const std::string& path,
                                              bool is_off_the_record,
                                              int request_id) {
  const std::string full_html = ResourceBundle::GetSharedInstance()
      .GetRawDataResource(IDR_TEXTFIELDS_HTML).as_string();

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

std::string TextfieldsUIHTMLSource::GetMimeType(
    const std::string& /* path */) const {
  return "text/html";
}

TextfieldsUIHTMLSource::~TextfieldsUIHTMLSource() {}

/**
 * TextfieldsDOMHandler implementation.
 */
TextfieldsDOMHandler::TextfieldsDOMHandler() : WebUIMessageHandler() {}

void TextfieldsDOMHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("textfieldValue",
      NewCallback(this, &TextfieldsDOMHandler::HandleTextfieldValue));
}

void TextfieldsDOMHandler::HandleTextfieldValue(const ListValue* args) {
  static_cast<TextfieldsUI*>(web_ui_)->set_text(ExtractStringValue(args));
}

/**
 * TextfieldsUI implementation.
 */
TextfieldsUI::TextfieldsUI(TabContents* contents) : WebUI(contents) {
  TextfieldsDOMHandler* handler = new TextfieldsDOMHandler();
  AddMessageHandler(handler);
  handler->Attach(this);

  TextfieldsUIHTMLSource* html_source = new TextfieldsUIHTMLSource();

  // Set up the chrome://textfields/ source.
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);
}
