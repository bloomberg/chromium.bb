// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/generic_handler.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"

GenericHandler::GenericHandler() {
}

GenericHandler::~GenericHandler() {
}

void GenericHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("navigateToUrl",
      NewCallback(this, &GenericHandler::HandleNavigateToUrl));
}

void GenericHandler::HandleNavigateToUrl(const ListValue* args) {
  std::string url_string;
  CHECK(args->GetString(0, &url_string));
  std::string button_string;
  CHECK(args->GetString(1, &button_string));
  CHECK(button_string == "0" || button_string == "1");
  std::string ctrl_string;
  CHECK(args->GetString(2, &ctrl_string));
  std::string shift_string;
  CHECK(args->GetString(3, &shift_string));
  std::string alt_string;
  CHECK(args->GetString(4, &alt_string));

  // TODO(estade): This logic is replicated in 4 places throughout chrome.
  // It would be nice to centralize it.
  WindowOpenDisposition disposition;
  if (button_string == "1" || ctrl_string == "true") {
    disposition = shift_string == "true" ?
        NEW_FOREGROUND_TAB : NEW_BACKGROUND_TAB;
  } else if (shift_string == "true") {
    disposition = NEW_WINDOW;
  } else {
    disposition = alt_string == "true" ? SAVE_TO_DISK : CURRENT_TAB;
  }

  web_ui_->tab_contents()->OpenURL(
      GURL(url_string), GURL(), disposition, PageTransition::LINK);

  // This may delete us!
}
