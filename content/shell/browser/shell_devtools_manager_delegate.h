// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_DEVTOOLS_MANAGER_DELEGATE_H_
#define CONTENT_SHELL_BROWSER_SHELL_DEVTOOLS_MANAGER_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {

class BrowserContext;

class ShellDevToolsManagerDelegate : public DevToolsManagerDelegate {
 public:
  static void StartHttpHandler(BrowserContext* browser_context);
  static void StopHttpHandler();
  static int GetHttpHandlerPort();

  explicit ShellDevToolsManagerDelegate(BrowserContext* browser_context);
  ~ShellDevToolsManagerDelegate() override;

  // DevToolsManagerDelegate implementation.
  scoped_refptr<DevToolsAgentHost> CreateNewTarget(const GURL& url) override;
  std::string GetDiscoveryPageHTML() override;
  bool HasBundledFrontendResources() override;

 private:
  BrowserContext* browser_context_;
  DISALLOW_COPY_AND_ASSIGN(ShellDevToolsManagerDelegate);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_DEVTOOLS_MANAGER_DELEGATE_H_
