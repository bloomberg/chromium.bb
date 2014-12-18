// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/browser_plugin_delegate.h"

namespace content {

bool BrowserPluginDelegate::OnMessageReceived(const IPC::Message& message) {
  return false;
}

}  // namespace content
