// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_RESOURCE_PROTOCOLS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_RESOURCE_PROTOCOLS_H_

#include "net/url_request/url_request_job_factory.h"

// Creates the handlers for the chrome-extension-resource:// scheme.
net::URLRequestJobFactory::ProtocolHandler*
CreateExtensionResourceProtocolHandler();

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_RESOURCE_PROTOCOLS_H_
