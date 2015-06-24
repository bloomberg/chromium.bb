// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extensions_browser_client.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "extensions/browser/extension_error.h"

namespace extensions {

namespace {

ExtensionsBrowserClient* g_client = NULL;

}  // namespace

void ExtensionsBrowserClient::ReportError(content::BrowserContext* context,
                                          scoped_ptr<ExtensionError> error) {
  LOG(ERROR) << error->GetDebugString();
}

ExtensionsBrowserClient* ExtensionsBrowserClient::Get() {
  return g_client;
}

void ExtensionsBrowserClient::Set(ExtensionsBrowserClient* client) {
  g_client = client;
}

}  // namespace extensions
