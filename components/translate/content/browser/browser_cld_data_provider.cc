// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/browser_cld_data_provider.h"

namespace translate {

bool BrowserCldDataProvider::OnMessageReceived(const IPC::Message& message) {
  return false;  // Message not handled
}

}  // namespace translate
