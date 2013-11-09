// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PROTOCOLS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PROTOCOLS_H_

#include "net/url_request/url_request_job_factory.h"

namespace extensions {
class InfoMap;
}

// Creates the handlers for the chrome-extension:// scheme.
net::URLRequestJobFactory::ProtocolHandler* CreateExtensionProtocolHandler(
    bool is_incognito,
    extensions::InfoMap* extension_info_map);

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PROTOCOLS_H_
