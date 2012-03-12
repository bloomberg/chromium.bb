// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_BROWSER_LIST_TABCONTENTS_PROVIDER_H_
#define CHROME_BROWSER_DEBUGGER_BROWSER_LIST_TABCONTENTS_PROVIDER_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/browser/devtools_http_handler_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {
class WebContents;
}

class BrowserListTabContentsProvider
    : public content::DevToolsHttpHandlerDelegate,
      public content::NotificationObserver {
 public:
  BrowserListTabContentsProvider();
  virtual ~BrowserListTabContentsProvider();

  // DevToolsHttpProtocolHandler::Delegate overrides.
  virtual DevToolsHttpHandlerDelegate::InspectableTabs
      GetInspectableTabs() OVERRIDE;
  virtual std::string GetDiscoveryPageHTML() OVERRIDE;
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
  virtual bool BundlesFrontendResources() OVERRIDE;
  virtual std::string GetFrontendResourcesBaseURL() OVERRIDE;

 private:
  // content::NotificationObserver overrides.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // The set of tab contents which can be inspected.
  std::set<content::WebContents*> contents_;

  // A scoped container for notification registries.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserListTabContentsProvider);
};

#endif  // CHROME_BROWSER_DEBUGGER_BROWSER_LIST_TABCONTENTS_PROVIDER_H_
