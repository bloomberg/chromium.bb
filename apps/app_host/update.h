// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_HOST_UPDATE_H_
#define APPS_APP_HOST_UPDATE_H_

namespace app_host {

// If system-level Chrome Binary is installed, and if its version is
// newer than App Host's, then udpate App Host by calling setup.exe.
void EnsureAppHostUpToDate();

}  // namespace app_host

#endif  // APPS_APP_HOST_UPDATE_H_
