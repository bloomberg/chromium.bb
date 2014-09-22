// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"

#include "base/values.h"
#include "chrome/browser/devtools/devtools_target_impl.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"

ChromeDevToolsManagerDelegate::ChromeDevToolsManagerDelegate()
    : network_protocol_handler_(new DevToolsNetworkProtocolHandler()) {
}

ChromeDevToolsManagerDelegate::~ChromeDevToolsManagerDelegate() {
}

void ChromeDevToolsManagerDelegate::Inspect(
    content::BrowserContext* browser_context,
    content::DevToolsAgentHost* agent_host) {
  if (!agent_host->IsWorker()) {
    // TODO(horo): Support other types of DevToolsAgentHost when necessary.
    NOTREACHED() << "Inspect() only supports workers.";
  }
  if (Profile* profile = Profile::FromBrowserContext(browser_context))
    DevToolsWindow::OpenDevToolsWindowForWorker(profile, agent_host);
}

base::DictionaryValue* ChromeDevToolsManagerDelegate::HandleCommand(
    content::DevToolsAgentHost* agent_host,
    base::DictionaryValue* command_dict) {
  return network_protocol_handler_->HandleCommand(agent_host, command_dict);
}

void ChromeDevToolsManagerDelegate::DevToolsAgentStateChanged(
    content::DevToolsAgentHost* agent_host,
    bool attached) {
  network_protocol_handler_->DevToolsAgentStateChanged(agent_host, attached);
}

std::string ChromeDevToolsManagerDelegate::GetPageThumbnailData(
    const GURL& url) {
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    Profile* profile = (*it)->profile();
    history::TopSites* top_sites = profile->GetTopSites();
    if (!top_sites)
      continue;
    scoped_refptr<base::RefCountedMemory> data;
    if (top_sites->GetPageThumbnail(url, false, &data))
      return std::string(data->front_as<char>(), data->size());
  }
  return std::string();
}

scoped_ptr<content::DevToolsTarget>
ChromeDevToolsManagerDelegate::CreateNewTarget(const GURL& url) {
  chrome::NavigateParams params(ProfileManager::GetLastUsedProfile(),
      url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
  if (!params.target_contents)
    return scoped_ptr<content::DevToolsTarget>();
  return scoped_ptr<content::DevToolsTarget>(
      DevToolsTargetImpl::CreateForWebContents(params.target_contents, true));
}

void ChromeDevToolsManagerDelegate::EnumerateTargets(TargetCallback callback) {
  DevToolsTargetImpl::EnumerateAllTargets(
      *reinterpret_cast<DevToolsTargetImpl::Callback*>(&callback));
}

