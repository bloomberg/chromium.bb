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
class RenderViewHost;
class WebContents;
}

class DevToolsTargetImpl : public content::DevToolsTarget {
 public:
  explicit DevToolsTargetImpl(
      scoped_refptr<content::DevToolsAgentHost> agent_host);
  virtual ~DevToolsTargetImpl();

  // content::DevToolsTarget overrides:
  virtual std::string GetId() const OVERRIDE;
  virtual std::string GetParentId() const OVERRIDE;
  virtual std::string GetType() const OVERRIDE;
  virtual std::string GetTitle() const OVERRIDE;
  virtual std::string GetDescription() const OVERRIDE;
  virtual GURL GetURL() const OVERRIDE;
  virtual GURL GetFaviconURL() const OVERRIDE;
  virtual base::TimeTicks GetLastActivityTime() const OVERRIDE;
  virtual scoped_refptr<content::DevToolsAgentHost>
      GetAgentHost() const OVERRIDE;
  virtual bool IsAttached() const OVERRIDE;
  virtual bool Activate() const OVERRIDE;
  virtual bool Close() const OVERRIDE;

  // Returns the WebContents associated with the target on NULL if there is
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
      content::WebContents* web_contents,
      bool is_tab);

  void set_parent_id(const std::string& parent_id) { parent_id_ = parent_id; }
  void set_type(const std::string& type) { type_ = type; }
  void set_title(const std::string& title) { title_ = title; }
  void set_description(const std::string& desc) { description_ = desc; }
  void set_url(const GURL& url) { url_ = url; }
  void set_favicon_url(const GURL& url) { favicon_url_ = url; }
  void set_last_activity_time(const base::TimeTicks& time) {
     last_activity_time_ = time;
  }

  typedef std::vector<DevToolsTargetImpl*> List;
  typedef base::Callback<void(const List&)> Callback;

  static List EnumerateWebContentsTargets();
  static void EnumerateWorkerTargets(Callback callback);
  static void EnumerateAllTargets(Callback callback);

 private:
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  std::string parent_id_;
  std::string type_;
  std::string title_;
  std::string description_;
  GURL url_;
  GURL favicon_url_;
  base::TimeTicks last_activity_time_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_TARGET_IMPL_H_
