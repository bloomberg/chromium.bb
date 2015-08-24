// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_dialogs.h"

#include "chrome/browser/ui/login/login_prompt.h"

// This file provides definitions of desktop browser dialog-creation methods for
// all toolkit-views platforms other than Mac. It also provides the definitions
// on Mac when mac_views_browser=1 is specified in GYP_DEFINES. The file is
// excluded in a Mac Cocoa build: definitions under chrome/browser/ui/cocoa may
// select at runtime whether to show a Cocoa dialog, or the toolkit-views dialog
// provided by browser_dialogs.h.

// static
LoginHandler* LoginHandler::Create(net::AuthChallengeInfo* auth_info,
                                   net::URLRequest* request) {
  return chrome::CreateLoginHandlerViews(auth_info, request);
}

// static
void BookmarkEditor::Show(gfx::NativeWindow parent_window,
                          Profile* profile,
                          const EditDetails& details,
                          Configuration configuration) {
  chrome::ShowBookmarkEditorViews(parent_window, profile, details,
                                  configuration);
}
