// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_BROWSER_LIST_TABCONTENTS_PROVIDER_H_
#define CHROME_BROWSER_DEBUGGER_BROWSER_LIST_TABCONTENTS_PROVIDER_H_

#include <string>
#include "content/browser/debugger/devtools_http_protocol_handler.h"

class BrowserListTabContentsProvider
    : public DevToolsHttpProtocolHandler::Delegate {
 public:
  BrowserListTabContentsProvider() {}
  virtual ~BrowserListTabContentsProvider() {}

  // DevToolsHttpProtocolHandler::Delegate overrides.
  virtual DevToolsHttpProtocolHandler::InspectableTabs
      GetInspectableTabs() OVERRIDE;
  virtual std::string GetDiscoveryPageHTML() OVERRIDE;
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserListTabContentsProvider);
};

#endif  // CHROME_BROWSER_DEBUGGER_BROWSER_LIST_TABCONTENTS_PROVIDER_H_
