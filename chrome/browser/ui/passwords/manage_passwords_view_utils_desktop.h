// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_VIEW_UTILS_DESKTOP_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_VIEW_UTILS_DESKTOP_H_

#include "base/strings/string16.h"

namespace syncer {
class SyncService;
}

// Returns the id of a string which should be displayed as a description for the
// password manager setting.
int GetPasswordManagerSettingsStringId(const syncer::SyncService* sync_service);

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_VIEW_UTILS_DESKTOP_H_
