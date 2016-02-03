// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

#include "base/files/file_path.h"
#include "ios/public/provider/web/web_ui_ios.h"
#include "ios/web/public/web_state/web_state.h"

namespace ios {

// static
ChromeBrowserState* ChromeBrowserState::FromBrowserState(
    web::BrowserState* browser_state) {
  // This is safe; this is the only implementation of BrowserState.
  return static_cast<ChromeBrowserState*>(browser_state);
}

// static
ChromeBrowserState* ChromeBrowserState::FromWebUIIOS(web::WebUIIOS* web_ui) {
  return FromBrowserState(web_ui->GetWebState()->GetBrowserState());
}

std::string ChromeBrowserState::GetDebugName() {
  // The debug name is based on the state path of the original browser state
  // to keep in sync with the meaning on other platforms.
  std::string name =
      GetOriginalChromeBrowserState()->GetStatePath().BaseName().MaybeAsASCII();
  if (name.empty()) {
    name = "UnknownBrowserState";
  }
  return name;
}

}  // namespace ios
