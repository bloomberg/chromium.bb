// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_display/display_info_provider.h"

namespace extensions {

// static
DisplayInfoProvider* DisplayInfoProvider::GetDisplayInfo() {
  return DisplayInfoProvider::GetInstance<DisplayInfoProvider>();
}

}  // namespace extensions

