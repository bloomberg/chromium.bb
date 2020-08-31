// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_INSTALL_APP_H_
#define CHROME_UPDATER_WIN_INSTALL_APP_H_

#include <string>

#include "base/memory/scoped_refptr.h"

namespace updater {

class App;

scoped_refptr<App> AppInstallInstance();

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_INSTALL_APP_H_
