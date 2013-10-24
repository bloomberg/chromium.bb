// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TARGET_IMPL_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TARGET_IMPL_H_

#include <vector>

#include "base/callback.h"
#include "content/public/browser/devtools_target.h"
#include "content/public/browser/worker_service.h"

class Profile;

namespace content {
class DevToolsAgentHost;
class WebContents;
}

class DevToolsTargetImpl : public content::DevToolsTarget {
 public:

  DevToolsTargetImpl();
  virtual ~DevToolsTargetImpl();

  // content::DevToolsTarget overrides:
  virtual std::string GetId() const OVERRIDE;
  virtual std::string GetType() const OVERRIDE;
  virtual std::string GetTitle() const OVERRIDE;
  virtual std::string GetDescription() const OVERRIDE;
  virtual GURL GetUrl() const OVERRIDE;
  virtual GURL GetFaviconUrl() const OVERRIDE;
  virtual base::TimeTicks GetLastActivityTime() const OVERRIDE;
  virtual bool IsAttached() const OVERRIDE;
  virtual scoped_refptr<content::DevToolsAgentHost> GetAgentHost() const
    OVERRIDE;
  virtual bool Activate() const OVERRIDE;
  virtual bool Close() const OVERRIDE;

  // Returns the WebContents is associated with the target on NULL if there is
  // not any.
  virtual content::WebContents* GetWebContents() const;

  // Returns the tab id if the target is associated with a tab, -1 otherwise.
  virtual int GetTabId() const;

  // Returns the extension id if the target is associated with an extension
  // background page.
  virtual std::string GetExtensionId() const;

  // Open a new DevTools window or activate the existing one.
  virtual void Inspect(Profile* profile) const;

  // Reload the target page.
  virtual void Reload() const;

  // Creates a new target associated with WebContents.
  static scoped_ptr<DevToolsTargetImpl> CreateForWebContents(
      content::WebContents*);

  // Creates a new target associated with a shared worker.
  static scoped_ptr<DevToolsTargetImpl> CreateForWorker(
      const content::WorkerService::WorkerInfo&);

  typedef std::vector<DevToolsTargetImpl*> List;
  typedef base::Callback<void(const List&)> Callback;

  static List EnumerateWebContentsTargets();
  static void EnumerateWorkerTargets(Callback callback);
  static void EnumerateAllTargets(Callback callback);

 protected:
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  std::string id_;
  std::string type_;
  std::string title_;
  std::string description_;
  GURL url_;
  GURL favicon_url_;
  base::TimeTicks last_activity_time_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TARGET_IMPL_H_
