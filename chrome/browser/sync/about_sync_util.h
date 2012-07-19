// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ABOUT_SYNC_UTIL_H_
#define CHROME_BROWSER_SYNC_ABOUT_SYNC_UTIL_H_

#include "base/memory/scoped_ptr.h"

class ProfileSyncService;

namespace base {
class DictionaryValue;
}

namespace sync_ui_util {
// This function returns a DictionaryValue which contains all the information
// required to populate the 'About' tab of about:sync.
// Note that |service| may be NULL.
scoped_ptr<base::DictionaryValue> ConstructAboutInformation(
    ProfileSyncService* service);
}

#endif  // CHROME_BROWSER_SYNC_ABOUT_SYNC_UTIL_H_
