// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_DEV_TOOLS_MANAGER_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_DEV_TOOLS_MANAGER_DELEGATE_H_

#include "base/basictypes.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace android_webview {

class AwDevToolsManagerDelegate : public content::DevToolsManagerDelegate {
 public:
  AwDevToolsManagerDelegate();
  virtual ~AwDevToolsManagerDelegate();

  // content::DevToolsManagerDelegate implementation.
  virtual void Inspect(
      content::BrowserContext* browser_context,
      content::DevToolsAgentHost* agent_host) override {}
  virtual void DevToolsAgentStateChanged(
      content::DevToolsAgentHost* agent_host,
      bool attached) override {}
  virtual base::DictionaryValue* HandleCommand(
      content::DevToolsAgentHost* agent_host,
      base::DictionaryValue* command_dict) override;
  virtual scoped_ptr<content::DevToolsTarget> CreateNewTarget(
      const GURL& url) override;
  virtual void EnumerateTargets(TargetCallback callback) override;
  virtual std::string GetPageThumbnailData(const GURL& url) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AwDevToolsManagerDelegate);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_DEV_TOOLS_MANAGER_DELEGATE_H_
