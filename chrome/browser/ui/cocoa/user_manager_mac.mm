// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/browser_dialogs.h"

namespace chrome {

// Declared in browser_dialogs.h so others don't have to depend on this header.
// TODO(noms): Add implementation when the User Manager dialog is implemented.
void ShowUserManager(const base::FilePath& profile_path_to_focus) {
  NOTIMPLEMENTED();
}

void HideUserManager() {
  NOTIMPLEMENTED();
}

}  // namespace chrome
