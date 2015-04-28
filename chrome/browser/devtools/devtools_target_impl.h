// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TARGET_IMPL_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TARGET_IMPL_H_

#include <vector>

#include "base/callback.h"
#include "components/devtools_discovery/basic_target_descriptor.h"

class Profile;

namespace content {
class DevToolsAgentHost;
class WebContents;
}

class DevToolsTargetImpl : public devtools_discovery::BasicTargetDescriptor {
 public:
  static const char kTargetTypeApp[];
  static const char kTargetTypeBackgroundPage[];
  static const char kTargetTypePage[];
  static const char kTargetTypeWorker[];
  static const char kTargetTypeWebView[];
  static const char kTargetTypeIFrame[];
  static const char kTargetTypeOther[];
  static const char kTargetTypeServiceWorker[];

  explicit DevToolsTargetImpl(
      scoped_refptr<content::DevToolsAgentHost> agent_host);
  ~DevToolsTargetImpl() override;

  // Returns the WebContents associated with the target on NULL if there is
  // not any.
  content::WebContents* GetWebContents() const;

  // Returns the tab id if the target is associated with a tab, -1 otherwise.
  virtual int GetTabId() const;

  // Returns the extension id if the target is associated with an extension
  // background page.
  virtual std::string GetExtensionId() const;

  // Open a new DevTools window or activate the existing one.
  virtual void Inspect(Profile* profile) const;

  // Reload the target page.
  virtual void Reload() const;

  // Creates a new target associated with tab.
  static scoped_ptr<DevToolsTargetImpl> CreateForTab(
      content::WebContents* web_contents);

  // Caller takes ownership of returned objects.
  static std::vector<DevToolsTargetImpl*> EnumerateAll();
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TARGET_IMPL_H_
