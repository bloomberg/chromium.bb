// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_MANAGER_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/devtools/devtools_network_protocol_handler.h"
#include "content/public/browser/devtools_manager_delegate.h"

class ChromeDevToolsManagerDelegate : public content::DevToolsManagerDelegate {
 public:
  ChromeDevToolsManagerDelegate();
  virtual ~ChromeDevToolsManagerDelegate();

  // content::DevToolsManagerDelegate implementation.
  virtual void Inspect(content::BrowserContext* browser_context,
                       content::DevToolsAgentHost* agent_host) override;
  virtual void DevToolsAgentStateChanged(content::DevToolsAgentHost* agent_host,
                                         bool attached) override;
  virtual base::DictionaryValue* HandleCommand(
      content::DevToolsAgentHost* agent_host,
      base::DictionaryValue* command_dict) override;
  virtual scoped_ptr<content::DevToolsTarget> CreateNewTarget(
      const GURL& url) override;
  virtual void EnumerateTargets(TargetCallback callback) override;
  virtual std::string GetPageThumbnailData(const GURL& url) override;

 private:
  scoped_ptr<DevToolsNetworkProtocolHandler> network_protocol_handler_;

  DISALLOW_COPY_AND_ASSIGN(ChromeDevToolsManagerDelegate);
};

#endif  // CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_MANAGER_DELEGATE_H_
