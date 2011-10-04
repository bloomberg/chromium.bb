// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/textfields_ui.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/singleton.h"
#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
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
                                              bool is_incognito,
                                              int request_id) {
  SendResponse(request_id, ResourceBundle::GetSharedInstance()
                           .LoadDataResourceBytes(IDR_TEXTFIELDS_HTML));
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
      base::Bind(&TextfieldsDOMHandler::HandleTextfieldValue,
                 base::Unretained(this)));
}

void TextfieldsDOMHandler::HandleTextfieldValue(const ListValue* args) {
  static_cast<TextfieldsUI*>(web_ui_)->set_text(
      UTF16ToWideHack(ExtractStringValue(args)));
}

/**
 * TextfieldsUI implementation.
 */
TextfieldsUI::TextfieldsUI(TabContents* contents) : ChromeWebUI(contents) {
  TextfieldsDOMHandler* handler = new TextfieldsDOMHandler();
  AddMessageHandler(handler);
  handler->Attach(this);

  TextfieldsUIHTMLSource* html_source = new TextfieldsUIHTMLSource();

  // Set up the chrome://textfields/ source.
  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  profile->GetChromeURLDataManager()->AddDataSource(html_source);
}
