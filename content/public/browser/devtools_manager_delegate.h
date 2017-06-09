// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_DELEGATE_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/devtools_agent_host.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}

namespace content {

class WebContents;

class CONTENT_EXPORT DevToolsManagerDelegate {
 public:
  // Opens the inspector for |agent_host|.
  virtual void Inspect(DevToolsAgentHost* agent_host);

  // Returns DevToolsAgentHost type to use for given |web_contents| target.
  virtual std::string GetTargetType(WebContents* web_contents);

  // Returns DevToolsAgentHost title to use for given |web_contents| target.
  virtual std::string GetTargetTitle(WebContents* web_contents);

  // Returns DevToolsAgentHost title to use for given |web_contents| target.
  virtual std::string GetTargetDescription(WebContents* web_contents);

  // Returns all targets embedder would like to report as debuggable
  // remotely.
  virtual DevToolsAgentHost::List RemoteDebuggingTargets();

  // Creates new inspectable target given the |url|.
  virtual scoped_refptr<DevToolsAgentHost> CreateNewTarget(const GURL& url);

  // Result ownership is passed to the caller.
  virtual base::DictionaryValue* HandleCommand(
      DevToolsAgentHost* agent_host,
      base::DictionaryValue* command);

  using CommandCallback =
      base::Callback<void(std::unique_ptr<base::DictionaryValue> response)>;
  virtual bool HandleAsyncCommand(DevToolsAgentHost* agent_host,
                                  base::DictionaryValue* command,
                                  const CommandCallback& callback);
  // Should return discovery page HTML that should list available tabs
  // and provide attach links.
  virtual std::string GetDiscoveryPageHTML();

  // Returns frontend resource data by |path|.
  virtual std::string GetFrontendResource(const std::string& path);

  virtual ~DevToolsManagerDelegate();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_DELEGATE_H_
