// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_network_protocol_handler.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"

char ChromeDevToolsManagerDelegate::kTypeApp[] = "app";
char ChromeDevToolsManagerDelegate::kTypeBackgroundPage[] = "background_page";
char ChromeDevToolsManagerDelegate::kTypeWebView[] = "webview";

ChromeDevToolsManagerDelegate::ChromeDevToolsManagerDelegate()
    : network_protocol_handler_(new DevToolsNetworkProtocolHandler()) {
  content::DevToolsAgentHost::AddDiscoveryProvider(
      base::Bind(&content::DevToolsAgentHost::GetOrCreateAll));
}

ChromeDevToolsManagerDelegate::~ChromeDevToolsManagerDelegate() {
}

void ChromeDevToolsManagerDelegate::Inspect(
    content::DevToolsAgentHost* agent_host) {
  Profile* profile =
      Profile::FromBrowserContext(agent_host->GetBrowserContext());
  if (!profile)
    return;
  std::string type = agent_host->GetType();
  if (type == content::DevToolsAgentHost::kTypeSharedWorker ||
      type == content::DevToolsAgentHost::kTypeServiceWorker) {
    DevToolsWindow::OpenDevToolsWindowForWorker(profile, agent_host);
    return;
  }
  content::WebContents* web_contents = agent_host->GetWebContents();
  if (web_contents)
    DevToolsWindow::OpenDevToolsWindow(web_contents);
}

base::DictionaryValue* ChromeDevToolsManagerDelegate::HandleCommand(
    content::DevToolsAgentHost* agent_host,
    base::DictionaryValue* command_dict) {
  return network_protocol_handler_->HandleCommand(agent_host, command_dict);
}

std::string ChromeDevToolsManagerDelegate::GetTargetType(
    content::RenderFrameHost* host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(host);

  guest_view::GuestViewBase* guest =
      guest_view::GuestViewBase::FromWebContents(web_contents);
  content::WebContents* guest_contents =
      guest ? guest->embedder_web_contents() : nullptr;
  if (guest_contents)
    return kTypeWebView;

  if (host->GetParent())
    return content::DevToolsAgentHost::kTypeFrame;

  for (TabContentsIterator it; !it.done(); it.Next()) {
    if (*it == web_contents)
      return content::DevToolsAgentHost::kTypePage;
  }

  const extensions::Extension* extension = extensions::ExtensionRegistry::Get(
      web_contents->GetBrowserContext())->enabled_extensions().GetByID(
          host->GetLastCommittedURL().host());
  if (!extension)
    return content::DevToolsAgentHost::kTypeOther;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile)
    return content::DevToolsAgentHost::kTypeOther;

  extensions::ExtensionHost* extension_host =
      extensions::ProcessManager::Get(profile)
          ->GetBackgroundHostForExtension(extension->id());
  if (extension_host &&
      extension_host->host_contents() == web_contents) {
    return kTypeBackgroundPage;
  } else if (extension->is_hosted_app()
             || extension->is_legacy_packaged_app()
             || extension->is_platform_app()) {
    return kTypeApp;
  }
  return content::DevToolsAgentHost::kTypeOther;
}

std::string ChromeDevToolsManagerDelegate::GetTargetTitle(
    content::RenderFrameHost* host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(host);
  if (host->GetParent())
    return host->GetLastCommittedURL().spec();
  for (TabContentsIterator it; !it.done(); it.Next()) {
    if (*it == web_contents)
      return base::UTF16ToUTF8(web_contents->GetTitle());
  }
  const extensions::Extension* extension = extensions::ExtensionRegistry::Get(
    web_contents->GetBrowserContext())->enabled_extensions().GetByID(
          host->GetLastCommittedURL().host());
  if (extension)
    return extension->name();
  return "";
}

scoped_refptr<content::DevToolsAgentHost>
ChromeDevToolsManagerDelegate::CreateNewTarget(const GURL& url) {
  chrome::NavigateParams params(ProfileManager::GetLastUsedProfile(),
      url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
  if (!params.target_contents)
    return nullptr;
  return content::DevToolsAgentHost::GetOrCreateFor(params.target_contents);
}

void ChromeDevToolsManagerDelegate::DevToolsAgentStateChanged(
    content::DevToolsAgentHost* agent_host,
    bool attached) {
  network_protocol_handler_->DevToolsAgentStateChanged(agent_host, attached);
}
