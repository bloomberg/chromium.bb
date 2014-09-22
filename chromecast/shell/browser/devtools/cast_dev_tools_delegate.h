// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_SHELL_BROWSER_DEVTOOLS_CAST_DEV_TOOLS_DELEGATE_H_
#define CHROMECAST_SHELL_BROWSER_DEVTOOLS_CAST_DEV_TOOLS_DELEGATE_H_

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
  virtual ~CastDevToolsDelegate();

  // DevToolsHttpHandlerDelegate implementation.
  virtual std::string GetDiscoveryPageHTML() OVERRIDE;
  virtual bool BundlesFrontendResources() OVERRIDE;
  virtual base::FilePath GetDebugFrontendDir() OVERRIDE;
  virtual scoped_ptr<net::StreamListenSocket> CreateSocketForTethering(
      net::StreamListenSocket::Delegate* delegate,
      std::string* name) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastDevToolsDelegate);
};

class CastDevToolsManagerDelegate : public content::DevToolsManagerDelegate {
 public:
  CastDevToolsManagerDelegate();
  virtual ~CastDevToolsManagerDelegate();

  // DevToolsManagerDelegate implementation.
  virtual void Inspect(
      content::BrowserContext* browser_context,
      content::DevToolsAgentHost* agent_host) OVERRIDE {}
  virtual void DevToolsAgentStateChanged(
      content::DevToolsAgentHost* agent_host,
      bool attached) OVERRIDE {}
  virtual base::DictionaryValue* HandleCommand(
      content::DevToolsAgentHost* agent_host,
      base::DictionaryValue* command) OVERRIDE;
  virtual scoped_ptr<content::DevToolsTarget> CreateNewTarget(
      const GURL& url) OVERRIDE;
  virtual void EnumerateTargets(TargetCallback callback) OVERRIDE;
  virtual std::string GetPageThumbnailData(const GURL& url) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastDevToolsManagerDelegate);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_SHELL_BROWSER_DEVTOOLS_CAST_DEV_TOOLS_DELEGATE_H_
