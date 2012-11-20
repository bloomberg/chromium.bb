// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_display/display_info_provider.h"

namespace extensions {

// TODO(hongbo): implement X11 display info querying.
bool DisplayInfoProvider::QueryInfo(DisplayInfo* info) {
  return false;
}

}  // namespace extensions
