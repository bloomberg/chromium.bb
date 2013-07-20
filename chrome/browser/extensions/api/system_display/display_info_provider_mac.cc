// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_display/display_info_provider.h"

namespace extensions {

void DisplayInfoProvider::RequestInfo(const RequestInfoCallback& callback) {
  // Redirect the request to a worker pool thread.
  StartQueryInfo(callback);
}

void DisplayInfoProvider::SetInfo(
    const std::string& display_id,
    const api::system_display::DisplayProperties& info,
    const SetInfoCallback& callback) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, false, "Not implemented"));
}

// TODO(hongbo): implement display info querying on Mac OS X.
bool DisplayInfoProvider::QueryInfo() {
  return false;
}

}  // namespace extensions
