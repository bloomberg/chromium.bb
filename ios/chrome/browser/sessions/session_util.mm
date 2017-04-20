// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sessions/session_util.h"

#include "base/files/file_path.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#include "ios/web/public/browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace session_util {

// Deletes the file containing the commands for the last session.
void DeleteLastSession(web::BrowserState* browser_state) {
  NSString* state_path =
      base::SysUTF8ToNSString(browser_state->GetStatePath().AsUTF8Unsafe());
  [[SessionServiceIOS sharedService]
      deleteLastSessionFileInDirectory:state_path];
}

}  // namespace session_util
