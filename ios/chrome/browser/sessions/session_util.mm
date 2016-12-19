// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sessions/session_util.h"

#include "base/files/file_path.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/sessions/session_service.h"
#include "ios/web/public/browser_state.h"

namespace session_util {

// Deletes the file containing the commands for the last session.
void DeleteLastSession(web::BrowserState* browser_state) {
  SessionServiceIOS* session_service = [SessionServiceIOS sharedService];
  NSString* path =
      base::SysUTF8ToNSString(browser_state->GetStatePath().value());
  [session_service deleteLastSession:path];
}

}  // namespace session_util
