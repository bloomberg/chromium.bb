// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/web_page_screen.h"

namespace chromeos {

///////////////////////////////////////////////////////////////////////////////
// WebPageScreen, public:

WebPageScreen::WebPageScreen() {
}

WebPageScreen::~WebPageScreen() {
}

///////////////////////////////////////////////////////////////////////////////
// WebPageScreen, TabContentsDelegate implementation:

bool WebPageScreen::HandleContextMenu(const ContextMenuParams& params) {
  // Just return true because we don't want to show context menue.
  return true;
}

}  // namespace chromeos
