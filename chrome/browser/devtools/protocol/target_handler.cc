// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/target_handler.h"

#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "content/public/browser/devtools_agent_host.h"

TargetHandler::TargetHandler(protocol::UberDispatcher* dispatcher)
    : weak_factory_(this) {
  protocol::Target::Dispatcher::wire(dispatcher, this);
}

TargetHandler::~TargetHandler() {
  ChromeDevToolsManagerDelegate* delegate =
      ChromeDevToolsManagerDelegate::GetInstance();
  if (delegate)
    delegate->UpdateDeviceDiscovery();
  registrations_.clear();
}

protocol::Response TargetHandler::SetRemoteLocations(
    std::unique_ptr<protocol::Array<protocol::Target::RemoteLocation>>
        locations) {
  remote_locations_.clear();
  if (!locations)
    return protocol::Response::OK();

  for (size_t i = 0; i < locations->length(); ++i) {
    auto* item = locations->get(i);
    remote_locations_.insert(
        net::HostPortPair(item->GetHost(), item->GetPort()));
  }

  ChromeDevToolsManagerDelegate* delegate =
      ChromeDevToolsManagerDelegate::GetInstance();
  if (delegate)
    delegate->UpdateDeviceDiscovery();
  return protocol::Response::OK();
}

protocol::Response TargetHandler::CreateTarget(
    const std::string& url,
    protocol::Maybe<int> width,
    protocol::Maybe<int> height,
    protocol::Maybe<std::string> browser_context_id,
    protocol::Maybe<bool> enable_begin_frame_control,
    std::string* out_target_id) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (browser_context_id.isJust()) {
    std::string profile_id = browser_context_id.fromJust();
    auto it = registrations_.find(profile_id);
    if (it == registrations_.end()) {
      return protocol::Response::Error(
          "Failed to find browser context with id " + profile_id);
    }
    profile = it->second->profile();
  }

  NavigateParams params(profile, GURL(url), ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  Browser* target_browser = nullptr;
  // Find a browser to open a new tab.
  // We shouldn't use browser that is scheduled to close.
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile() == profile &&
        !browser->IsAttemptingToCloseBrowser()) {
      target_browser = browser;
      break;
    }
  }
  if (target_browser) {
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    params.browser = target_browser;
  } else {
    params.disposition = WindowOpenDisposition::NEW_WINDOW;
  }

  Navigate(&params);
  if (!params.navigated_or_inserted_contents)
    return protocol::Response::Error("Failed to open a new tab");

  *out_target_id = content::DevToolsAgentHost::GetOrCreateFor(
                       params.navigated_or_inserted_contents)
                       ->GetId();
  return protocol::Response::OK();
}

protocol::Response TargetHandler::CreateBrowserContext(
    std::string* out_context_id) {
  Profile* original_profile =
      ProfileManager::GetActiveUserProfile()->GetOriginalProfile();

  auto registration =
      IndependentOTRProfileManager::GetInstance()->CreateFromOriginalProfile(
          original_profile,
          base::BindOnce(&TargetHandler::OnOriginalProfileDestroyed,
                         weak_factory_.GetWeakPtr()));
  *out_context_id = registration->profile()->UniqueId();
  registrations_[*out_context_id] = std::move(registration);
  return protocol::Response::OK();
}

void TargetHandler::OnOriginalProfileDestroyed(Profile* profile) {
  base::EraseIf(registrations_, [&profile](const auto& it) {
    return it.second->profile()->GetOriginalProfile() == profile;
  });
}

protocol::Response TargetHandler::DisposeBrowserContext(
    const std::string& context_id,
    bool* out_success) {
  auto it = registrations_.find(context_id);
  if (it == registrations_.end()) {
    return protocol::Response::Error("Failed to find browser context with id " +
                                     context_id);
  }
  Profile* profile = it->second->profile();
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile() == profile &&
        !browser->IsAttemptingToCloseBrowser()) {
      *out_success = false;
      return protocol::Response::OK();
    }
  }
  registrations_.erase(it);
  *out_success = true;
  return protocol::Response::OK();
}
