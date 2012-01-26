// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_BROWSER_LIST_TABCONTENTS_PROVIDER_H_
#define CHROME_BROWSER_DEBUGGER_BROWSER_LIST_TABCONTENTS_PROVIDER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/browser/devtools_http_handler_delegate.h"

class BrowserListTabContentsProvider
    : public content::DevToolsHttpHandlerDelegate {
 public:
  BrowserListTabContentsProvider() {}
  virtual ~BrowserListTabContentsProvider() {}

  // DevToolsHttpProtocolHandler::Delegate overrides.
  virtual DevToolsHttpHandlerDelegate::InspectableTabs
      GetInspectableTabs() OVERRIDE;
  virtual std::string GetDiscoveryPageHTML() OVERRIDE;
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserListTabContentsProvider);
};

#endif  // CHROME_BROWSER_DEBUGGER_BROWSER_LIST_TABCONTENTS_PROVIDER_H_
