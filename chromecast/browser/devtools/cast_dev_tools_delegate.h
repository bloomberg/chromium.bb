// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_DEVTOOLS_CAST_DEV_TOOLS_DELEGATE_H_
#define CHROMECAST_BROWSER_DEVTOOLS_CAST_DEV_TOOLS_DELEGATE_H_

#include "content/public/browser/devtools_http_handler_delegate.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "net/socket/stream_listen_socket.h"

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace chromecast {
namespace shell {

class CastDevToolsDelegate : public content::DevToolsHttpHandlerDelegate {
 public:
  CastDevToolsDelegate();
  ~CastDevToolsDelegate() override;

  // DevToolsHttpHandlerDelegate implementation.
  std::string GetDiscoveryPageHTML() override;
  bool BundlesFrontendResources() override;
  base::FilePath GetDebugFrontendDir() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastDevToolsDelegate);
};

class CastDevToolsManagerDelegate : public content::DevToolsManagerDelegate {
 public:
  CastDevToolsManagerDelegate();
  ~CastDevToolsManagerDelegate() override;

  // DevToolsManagerDelegate implementation.
  void Inspect(
      content::BrowserContext* browser_context,
      content::DevToolsAgentHost* agent_host) override {}
  void DevToolsAgentStateChanged(
      content::DevToolsAgentHost* agent_host,
      bool attached) override {}
  base::DictionaryValue* HandleCommand(
      content::DevToolsAgentHost* agent_host,
      base::DictionaryValue* command) override;
  scoped_ptr<content::DevToolsTarget> CreateNewTarget(const GURL& url) override;
  void EnumerateTargets(TargetCallback callback) override;
  std::string GetPageThumbnailData(const GURL& url) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastDevToolsManagerDelegate);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_DEVTOOLS_CAST_DEV_TOOLS_DELEGATE_H_
