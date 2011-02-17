// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webui/keyboard_ui.h"

#include "base/ref_counted_memory.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

///////////////////////////////////////////////////////////////////////////////
// KeyboardUI

KeyboardUI::KeyboardUI(TabContents* contents)
    : WebUI(contents) {
  KeyboardHTMLSource* html_source = new KeyboardHTMLSource();
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);
}

KeyboardUI::~KeyboardUI() {
}

///////////////////////////////////////////////////////////////////////////////
// KeyboardHTMLSource

KeyboardUI::KeyboardHTMLSource::KeyboardHTMLSource()
    : DataSource(chrome::kChromeUIKeyboardHost, MessageLoop::current()) {
}

void KeyboardUI::KeyboardHTMLSource::StartDataRequest(const std::string& path,
                                                      bool is_off_the_record,
                                                      int request_id) {
  NOTREACHED() << "We should never get here since the extension should have"
               << "been triggered";
  SendResponse(request_id, NULL);
}

std::string KeyboardUI::KeyboardHTMLSource::GetMimeType(
    const std::string&) const {
  NOTREACHED() << "We should never get here since the extension should have"
               << "been triggered";
  return "text/html";
}
