// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_BROWSER_LIST_TABCONTENTS_PROVIDER_H_
#define CHROME_BROWSER_DEVTOOLS_BROWSER_LIST_TABCONTENTS_PROVIDER_H_

#include "chrome/browser/ui/host_desktop.h"
#include "content/public/browser/devtools_http_handler_delegate.h"

class BrowserListTabContentsProvider
    : public content::DevToolsHttpHandlerDelegate {
 public:
  explicit BrowserListTabContentsProvider(
      chrome::HostDesktopType host_desktop_type);
  ~BrowserListTabContentsProvider() override;

  // DevToolsHttpHandlerDelegate implementation.
  std::string GetDiscoveryPageHTML() override;
  bool BundlesFrontendResources() override;
  base::FilePath GetDebugFrontendDir() override;

 private:
  chrome::HostDesktopType host_desktop_type_;
  DISALLOW_COPY_AND_ASSIGN(BrowserListTabContentsProvider);
};

#endif  // CHROME_BROWSER_DEVTOOLS_BROWSER_LIST_TABCONTENTS_PROVIDER_H_
