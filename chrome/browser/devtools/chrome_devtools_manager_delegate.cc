// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_network_protocol_handler.h"
#include "components/devtools_discovery/devtools_discovery_manager.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#else  // !defined(OS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#endif  // !defined(OS_ANDROID)

using devtools_discovery::DevToolsDiscoveryManager;

char ChromeDevToolsManagerDelegate::kTypeApp[] = "app";
char ChromeDevToolsManagerDelegate::kTypeBackgroundPage[] = "background_page";

ChromeDevToolsManagerDelegate::ChromeDevToolsManagerDelegate()
    : network_protocol_handler_(new DevToolsNetworkProtocolHandler()) {
}

ChromeDevToolsManagerDelegate::~ChromeDevToolsManagerDelegate() {
}

void ChromeDevToolsManagerDelegate::Inspect(
    content::DevToolsAgentHost* agent_host) {
#if !defined(OS_ANDROID)
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
#endif  // !defined(OS_ANDROID)
}

base::DictionaryValue* ChromeDevToolsManagerDelegate::HandleCommand(
    content::DevToolsAgentHost* agent_host,
    base::DictionaryValue* command_dict) {
  std::unique_ptr<base::DictionaryValue> result =
      DevToolsDiscoveryManager::GetInstance()->HandleCreateTargetCommand(
          command_dict);
  if (result)
    return result.release();  // Caller takes ownership.
  return network_protocol_handler_->HandleCommand(agent_host, command_dict);
}

std::string ChromeDevToolsManagerDelegate::GetTargetType(
    content::RenderFrameHost* host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(host);
#if !defined(OS_ANDROID)
  for (TabContentsIterator it; !it.done(); it.Next()) {
    if (*it == web_contents)
      return content::DevToolsAgentHost::kTypePage;
  }

  if (host->GetParent())
    return content::DevToolsAgentHost::kTypeFrame;

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
#else  // !defined(OS_ANDROID)
  for (TabModelList::const_iterator iter = TabModelList::begin();
      iter != TabModelList::end(); ++iter) {
    TabModel* model = *iter;
    for (int i = 0; i < model->GetTabCount(); ++i) {
      TabAndroid* tab = model->GetTabAt(i);
      if (tab && web_contents == tab->web_contents())
        return content::DevToolsAgentHost::kTypePage;
    }
  }
#endif  // !defined(OS_ANDROID)
  return content::DevToolsAgentHost::kTypeOther;
}

std::string ChromeDevToolsManagerDelegate::GetTargetTitle(
    content::RenderFrameHost* host) {
#if !defined(OS_ANDROID)
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
#endif  // !defined(OS_ANDROID)
  return "";
}

void ChromeDevToolsManagerDelegate::DevToolsAgentStateChanged(
    content::DevToolsAgentHost* agent_host,
    bool attached) {
  network_protocol_handler_->DevToolsAgentStateChanged(agent_host, attached);
}
