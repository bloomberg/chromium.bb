// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/android/new_tab_page_ready_handler.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/android/tab_android.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

NewTabPageReadyHandler::NewTabPageReadyHandler() {
}

NewTabPageReadyHandler::~NewTabPageReadyHandler() {
}

void NewTabPageReadyHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("notifyNTPReady", base::Bind(
      &NewTabPageReadyHandler::HandleNewTabPageReady, base::Unretained(this)));
  web_ui()->RegisterMessageCallback("NTPUnexpectedNavigation", base::Bind(
      &NewTabPageReadyHandler::HandleNewTabPageUnexpectedNavigation,
      base::Unretained(this)));
  web_ui()->RegisterMessageCallback("notifyNTPTitleLoaded", base::Bind(
      &NewTabPageReadyHandler::HandleNewTabPageTitleLoaded,
      base::Unretained(this)));
}

void NewTabPageReadyHandler::HandleNewTabPageTitleLoaded(
    const ListValue* args) {
  web_ui()->OverrideTitle(base::string16());
}

void NewTabPageReadyHandler::HandleNewTabPageReady(
    const ListValue* args) {
  TabAndroid* tab = TabAndroid::FromWebContents(web_ui()->GetWebContents());
  if (!tab)
    return;
  tab->OnNewTabPageReady();
}

void NewTabPageReadyHandler::HandleNewTabPageUnexpectedNavigation(
    const ListValue* args) {
  // NTP reached an unexpected state trying to send finish loading notification
  // a second time.  The notification should be sent only when page is
  // completely done loading. This could otherwise create a race condition in
  // tests waiting for the NTP to have loaded (any navigation NTP does after
  // loading could interfere with the test navigation).
  NOTREACHED();
}
