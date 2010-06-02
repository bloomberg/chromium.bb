// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/web_page_screen.h"

#include "base/time.h"

using base::TimeDelta;

namespace chromeos {

namespace {

// Time in seconds after page load is considered timed out.
const int kNetworkTimeoutSec = 10;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// WebPageScreen, TabContentsDelegate implementation:

bool WebPageScreen::HandleContextMenu(const ContextMenuParams& params) {
  // Just return true because we don't want to show context menue.
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// WebPageScreen, protected:

void WebPageScreen::OnNetworkTimeout() {
  // TODO(nkostylev): Add better detection for limited connectivity.
  // http://crosbug.com/3690
  CloseScreen(ScreenObserver::CONNECTION_FAILED);
}

void WebPageScreen::StartTimeoutTimer() {
  StopTimeoutTimer();
  timeout_timer_.Start(TimeDelta::FromSeconds(kNetworkTimeoutSec),
                       this,
                       &WebPageScreen::OnNetworkTimeout);
}

void WebPageScreen::StopTimeoutTimer() {
  if (timeout_timer_.IsRunning())
    timeout_timer_.Stop();
}

}  // namespace chromeos
