// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extensions_browser_client.h"

#include "base/basictypes.h"

namespace extensions {

namespace {

ExtensionsBrowserClient* g_client = NULL;

}  // namespace

ExtensionsBrowserClient* ExtensionsBrowserClient::Get() {
  return g_client;
}

void ExtensionsBrowserClient::Set(ExtensionsBrowserClient* client) {
  g_client = client;
}

}  // namespace extensions
