// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_dev_tools_manager_delegate.h"

#include "android_webview/native/aw_contents.h"
#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_target.h"
#include "content/public/browser/web_contents.h"

using content::DevToolsAgentHost;
using content::RenderViewHost;
using content::WebContents;

namespace {

const char kTargetTypePage[] = "page";
const char kTargetTypeServiceWorker[] = "service_worker";
const char kTargetTypeOther[] = "other";

std::string GetViewDescription(WebContents* web_contents);

class Target : public content::DevToolsTarget {
 public:
  explicit Target(scoped_refptr<DevToolsAgentHost> agent_host);

  virtual std::string GetId() const OVERRIDE { return agent_host_->GetId(); }
  virtual std::string GetParentId() const OVERRIDE { return std::string(); }
  virtual std::string GetType() const OVERRIDE {
    switch (agent_host_->GetType()) {
      case DevToolsAgentHost::TYPE_WEB_CONTENTS:
        return kTargetTypePage;
      case DevToolsAgentHost::TYPE_SERVICE_WORKER:
        return kTargetTypeServiceWorker;
      default:
        break;
    }
    return kTargetTypeOther;
  }
  virtual std::string GetTitle() const OVERRIDE {
    return agent_host_->GetTitle();
  }
  virtual std::string GetDescription() const OVERRIDE { return description_; }
  virtual GURL GetURL() const OVERRIDE { return agent_host_->GetURL(); }
  virtual GURL GetFaviconURL() const OVERRIDE { return GURL(); }
  virtual base::TimeTicks GetLastActivityTime() const OVERRIDE {
    return last_activity_time_;
  }
  virtual bool IsAttached() const OVERRIDE {
    return agent_host_->IsAttached();
  }
  virtual scoped_refptr<DevToolsAgentHost> GetAgentHost() const OVERRIDE {
    return agent_host_;
  }
  virtual bool Activate() const OVERRIDE { return agent_host_->Activate(); }
  virtual bool Close() const OVERRIDE { return agent_host_->Close(); }

 private:
  scoped_refptr<DevToolsAgentHost> agent_host_;
  std::string description_;
  base::TimeTicks last_activity_time_;
};

Target::Target(scoped_refptr<DevToolsAgentHost> agent_host)
    : agent_host_(agent_host) {
  if (WebContents* web_contents = agent_host->GetWebContents()) {
    description_ = GetViewDescription(web_contents);
    last_activity_time_ = web_contents->GetLastActiveTime();
  }
}

std::string GetViewDescription(WebContents* web_contents) {
  const android_webview::BrowserViewRenderer* bvr =
      android_webview::AwContents::FromWebContents(web_contents)
          ->GetBrowserViewRenderer();
  if (!bvr) return "";
  base::DictionaryValue description;
  description.SetBoolean("attached", bvr->attached_to_window());
  description.SetBoolean("visible", bvr->IsVisible());
  gfx::Rect screen_rect = bvr->GetScreenRect();
  description.SetInteger("screenX", screen_rect.x());
  description.SetInteger("screenY", screen_rect.y());
  description.SetBoolean("empty", screen_rect.size().IsEmpty());
  if (!screen_rect.size().IsEmpty()) {
    description.SetInteger("width", screen_rect.width());
    description.SetInteger("height", screen_rect.height());
  }
  std::string json;
  base::JSONWriter::Write(&description, &json);
  return json;
}

}  // namespace

namespace android_webview {

AwDevToolsManagerDelegate::AwDevToolsManagerDelegate() {
}

AwDevToolsManagerDelegate::~AwDevToolsManagerDelegate() {
}

base::DictionaryValue* AwDevToolsManagerDelegate::HandleCommand(
    content::DevToolsAgentHost* agent_host,
    base::DictionaryValue* command_dict) {
  return NULL;
}

void AwDevToolsManagerDelegate::EnumerateTargets(TargetCallback callback) {
  TargetList targets;
  DevToolsAgentHost::List agents = DevToolsAgentHost::GetOrCreateAll();
  for (DevToolsAgentHost::List::iterator it = agents.begin();
      it != agents.end(); ++it) {
    targets.push_back(new Target(*it));
  }
  callback.Run(targets);
}

std::string AwDevToolsManagerDelegate::GetPageThumbnailData(const GURL&) {
  return "";
}

scoped_ptr<content::DevToolsTarget> AwDevToolsManagerDelegate::CreateNewTarget(
    const GURL&) {
  return scoped_ptr<content::DevToolsTarget>();
}

}  // namespace android_webview
