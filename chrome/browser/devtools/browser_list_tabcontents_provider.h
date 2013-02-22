// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_BROWSER_LIST_TABCONTENTS_PROVIDER_H_
#define CHROME_BROWSER_DEVTOOLS_BROWSER_LIST_TABCONTENTS_PROVIDER_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/host_desktop.h"
#include "content/public/browser/devtools_http_handler_delegate.h"

class Profile;

class BrowserListTabContentsProvider
    : public content::DevToolsHttpHandlerDelegate {
 public:
  BrowserListTabContentsProvider(Profile* profile,
                                 chrome::HostDesktopType host_desktop_type);
  virtual ~BrowserListTabContentsProvider();

  // DevToolsHttpProtocolHandler::Delegate overrides.
  virtual std::string GetDiscoveryPageHTML() OVERRIDE;
  virtual bool BundlesFrontendResources() OVERRIDE;
  virtual base::FilePath GetDebugFrontendDir() OVERRIDE;
  virtual std::string GetPageThumbnailData(const GURL& url) OVERRIDE;
  virtual content::RenderViewHost* CreateNewTarget() OVERRIDE;
  virtual TargetType GetTargetType(content::RenderViewHost*) OVERRIDE;

 private:
  Profile* profile_;
  chrome::HostDesktopType host_desktop_type_;
  DISALLOW_COPY_AND_ASSIGN(BrowserListTabContentsProvider);
};

#endif  // CHROME_BROWSER_DEVTOOLS_BROWSER_LIST_TABCONTENTS_PROVIDER_H_
