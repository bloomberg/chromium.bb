// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_ABOUT_SYNC_UTIL_H_
#define COMPONENTS_SYNC_DRIVER_ABOUT_SYNC_UTIL_H_

#include "base/memory/scoped_ptr.h"
#include "components/version_info/version_info.h"

class SigninManagerBase;

namespace base {
class DictionaryValue;
}

namespace sync_driver {
class SyncService;
}

// These strings are used from logs to pull out specific data from sync; we
// don't want these to ever go out of sync between the logs and sync util.
extern const char kIdentityTitle[];
extern const char kDetailsKey[];

namespace sync_ui_util {
// This function returns a DictionaryValue which contains all the information
// required to populate the 'About' tab of about:sync.
// Note that |service| may be NULL.
scoped_ptr<base::DictionaryValue> ConstructAboutInformation(
    sync_driver::SyncService* service,
    SigninManagerBase* signin,
    version_info::Channel channel);
}

#endif  // COMPONENTS_SYNC_DRIVER_ABOUT_SYNC_UTIL_H_
