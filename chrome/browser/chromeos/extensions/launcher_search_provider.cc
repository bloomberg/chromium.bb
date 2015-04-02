// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/launcher_search_provider.h"

namespace extensions {

LauncherSearchProviderSetSearchResultsFunction::
    ~LauncherSearchProviderSetSearchResultsFunction() {
}

bool LauncherSearchProviderSetSearchResultsFunction::RunSync() {
  // TODO(yawano): Implement this (crbug.com/440649).
  NOTREACHED();
  return false;
}

}  // namespace extensions
