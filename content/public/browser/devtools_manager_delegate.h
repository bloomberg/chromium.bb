// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_DELEGATE_H_

#include <string>
#include <vector>

#include "base/callback.h"

namespace base {
class DictionaryValue;
}

class GURL;

namespace content {

class BrowserContext;
class DevToolsAgentHost;
class DevToolsTarget;

class DevToolsManagerDelegate {
 public:
  virtual ~DevToolsManagerDelegate() {}

  // Opens the inspector for |agent_host|.
  virtual void Inspect(BrowserContext* browser_context,
                       DevToolsAgentHost* agent_host) = 0;

  virtual void DevToolsAgentStateChanged(DevToolsAgentHost* agent_host,
                                         bool attached) = 0;

  // Result ownership is passed to the caller.
  virtual base::DictionaryValue* HandleCommand(
      DevToolsAgentHost* agent_host,
      base::DictionaryValue* command) = 0;

  // Creates new inspectable target.
  virtual scoped_ptr<DevToolsTarget> CreateNewTarget(const GURL& url) = 0;

  typedef std::vector<DevToolsTarget*> TargetList;
  typedef base::Callback<void(const TargetList&)> TargetCallback;

  // Requests the list of all inspectable targets.
  // The caller gets the ownership of the returned targets.
  virtual void EnumerateTargets(TargetCallback callback) = 0;

  // Get a thumbnail for a given page. Returns non-empty string iff we have the
  // thumbnail.
  virtual std::string GetPageThumbnailData(const GURL& url) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_DELEGATE_H_
