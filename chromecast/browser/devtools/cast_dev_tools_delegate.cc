// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/devtools/cast_dev_tools_delegate.h"

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_target.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "grit/shell_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromecast {
namespace shell {

namespace {

const char kTargetTypePage[] = "page";
const char kTargetTypeServiceWorker[] = "service_worker";
const char kTargetTypeSharedWorker[] = "worker";
const char kTargetTypeOther[] = "other";

class Target : public content::DevToolsTarget {
 public:
  explicit Target(scoped_refptr<content::DevToolsAgentHost> agent_host);

  std::string GetId() const override { return agent_host_->GetId(); }
  std::string GetParentId() const override { return std::string(); }
  std::string GetType() const override {
    switch (agent_host_->GetType()) {
      case content::DevToolsAgentHost::TYPE_WEB_CONTENTS:
        return kTargetTypePage;
      case content::DevToolsAgentHost::TYPE_SERVICE_WORKER:
        return kTargetTypeServiceWorker;
      case content::DevToolsAgentHost::TYPE_SHARED_WORKER:
        return kTargetTypeSharedWorker;
      default:
        break;
    }
    return kTargetTypeOther;
  }
  std::string GetTitle() const override { return agent_host_->GetTitle(); }
  std::string GetDescription() const override { return std::string(); }
  GURL GetURL() const override { return agent_host_->GetURL(); }
  GURL GetFaviconURL() const override { return favicon_url_; }
  base::TimeTicks GetLastActivityTime() const override {
    return last_activity_time_;
  }
  bool IsAttached() const override { return agent_host_->IsAttached(); }
  scoped_refptr<content::DevToolsAgentHost> GetAgentHost() const override {
    return agent_host_;
  }
  bool Activate() const override { return agent_host_->Activate(); }
  bool Close() const override { return agent_host_->Close(); }

 private:
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  GURL favicon_url_;
  base::TimeTicks last_activity_time_;

  DISALLOW_COPY_AND_ASSIGN(Target);
};

Target::Target(scoped_refptr<content::DevToolsAgentHost> agent_host)
    : agent_host_(agent_host) {
  if (content::WebContents* web_contents = agent_host_->GetWebContents()) {
    content::NavigationController& controller = web_contents->GetController();
    content::NavigationEntry* entry = controller.GetActiveEntry();
    if (entry != NULL && entry->GetURL().is_valid())
      favicon_url_ = entry->GetFavicon().url;
    last_activity_time_ = web_contents->GetLastActiveTime();
  }
}

}  // namespace

// CastDevToolsDelegate -----------------------------------------------------

CastDevToolsDelegate::CastDevToolsDelegate() {
}

CastDevToolsDelegate::~CastDevToolsDelegate() {
}

std::string CastDevToolsDelegate::GetDiscoveryPageHTML() {
#if defined(OS_ANDROID)
  return std::string();
#else
  return ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_CAST_SHELL_DEVTOOLS_DISCOVERY_PAGE).as_string();
#endif  // defined(OS_ANDROID)
}

bool CastDevToolsDelegate::BundlesFrontendResources() {
  return false;
}

base::FilePath CastDevToolsDelegate::GetDebugFrontendDir() {
  return base::FilePath();
}

// CastDevToolsManagerDelegate -----------------------------------------------

CastDevToolsManagerDelegate::CastDevToolsManagerDelegate() {
}

CastDevToolsManagerDelegate::~CastDevToolsManagerDelegate() {
}

base::DictionaryValue* CastDevToolsManagerDelegate::HandleCommand(
    content::DevToolsAgentHost* agent_host,
    base::DictionaryValue* command) {
  return NULL;
}

std::string CastDevToolsManagerDelegate::GetPageThumbnailData(
    const GURL& url) {
  return "";
}

scoped_ptr<content::DevToolsTarget>
CastDevToolsManagerDelegate::CreateNewTarget(const GURL& url) {
  return scoped_ptr<content::DevToolsTarget>();
}

void CastDevToolsManagerDelegate::EnumerateTargets(TargetCallback callback) {
  TargetList targets;
  content::DevToolsAgentHost::List agents =
      content::DevToolsAgentHost::GetOrCreateAll();
  for (content::DevToolsAgentHost::List::iterator it = agents.begin();
       it != agents.end(); ++it) {
    targets.push_back(new Target(*it));
  }
  callback.Run(targets);
}

}  // namespace shell
}  // namespace chromecast
