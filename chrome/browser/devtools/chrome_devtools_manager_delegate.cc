// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/device/android_device_manager.h"
#include "chrome/browser/devtools/device/tcp_device_provider.h"
#include "chrome/browser/devtools/devtools_network_protocol_handler.h"
#include "chrome/browser/devtools/devtools_protocol_constants.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/grit/browser_resources.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "ui/base/resource/resource_bundle.h"

using content::DevToolsAgentHost;

char ChromeDevToolsManagerDelegate::kTypeApp[] = "app";
char ChromeDevToolsManagerDelegate::kTypeBackgroundPage[] = "background_page";
char ChromeDevToolsManagerDelegate::kTypeWebView[] = "webview";

char kLocationsParam[] = "locations";
char kHostParam[] = "host";
char kPortParam[] = "port";

class ChromeDevToolsManagerDelegate::HostData {
 public:
  HostData() {}
  ~HostData() {}

  RemoteLocations& remote_locations() { return remote_locations_; }

  void set_remote_locations(RemoteLocations& locations) {
    remote_locations_.swap(locations);
  }

 private:
  RemoteLocations remote_locations_;
};

ChromeDevToolsManagerDelegate::ChromeDevToolsManagerDelegate()
    : network_protocol_handler_(new DevToolsNetworkProtocolHandler()) {
  content::DevToolsAgentHost::AddObserver(this);
}

ChromeDevToolsManagerDelegate::~ChromeDevToolsManagerDelegate() {
  content::DevToolsAgentHost::RemoveObserver(this);
}

void ChromeDevToolsManagerDelegate::Inspect(
    content::DevToolsAgentHost* agent_host) {
  DevToolsWindow::OpenDevToolsWindow(agent_host, nullptr);
}

base::DictionaryValue* ChromeDevToolsManagerDelegate::HandleCommand(
    DevToolsAgentHost* agent_host,
    base::DictionaryValue* command_dict) {

  int id = 0;
  std::string method;
  base::DictionaryValue* params = nullptr;
  if (!DevToolsProtocol::ParseCommand(command_dict, &id, &method, &params))
    return nullptr;

  if (method == chrome::devtools::Target::setRemoteLocations::kName)
    return SetRemoteLocations(agent_host, id, params).release();

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
    return DevToolsAgentHost::kTypeFrame;

  for (TabContentsIterator it; !it.done(); it.Next()) {
    if (*it == web_contents)
      return DevToolsAgentHost::kTypePage;
  }

  const extensions::Extension* extension = extensions::ExtensionRegistry::Get(
      web_contents->GetBrowserContext())->enabled_extensions().GetByID(
          host->GetLastCommittedURL().host());
  if (!extension)
    return DevToolsAgentHost::kTypeOther;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile)
    return DevToolsAgentHost::kTypeOther;

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
  return DevToolsAgentHost::kTypeOther;
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

scoped_refptr<DevToolsAgentHost>
ChromeDevToolsManagerDelegate::CreateNewTarget(const GURL& url) {
  chrome::NavigateParams params(ProfileManager::GetLastUsedProfile(),
      url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
  if (!params.target_contents)
    return nullptr;
  return DevToolsAgentHost::GetOrCreateFor(params.target_contents);
}

std::string ChromeDevToolsManagerDelegate::GetDiscoveryPageHTML() {
  return ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_DEVTOOLS_DISCOVERY_PAGE_HTML).as_string();
}

std::string ChromeDevToolsManagerDelegate::GetFrontendResource(
    const std::string& path) {
  return content::DevToolsFrontendHost::GetFrontendResource(path).as_string();
}

void ChromeDevToolsManagerDelegate::DevToolsAgentHostAttached(
    content::DevToolsAgentHost* agent_host) {
  network_protocol_handler_->DevToolsAgentStateChanged(agent_host, true);

  DCHECK(host_data_.find(agent_host) == host_data_.end());
  host_data_[agent_host].reset(new ChromeDevToolsManagerDelegate::HostData());
}

void ChromeDevToolsManagerDelegate::DevToolsAgentHostDetached(
    content::DevToolsAgentHost* agent_host) {
  network_protocol_handler_->DevToolsAgentStateChanged(agent_host, false);
  // This class is created lazily, so it may not know about some attached hosts.
  if (host_data_.find(agent_host) != host_data_.end()) {
    host_data_.erase(agent_host);
    UpdateDeviceDiscovery();
  }
}

void ChromeDevToolsManagerDelegate::DevicesAvailable(
    const DevToolsDeviceDiscovery::CompleteDevices& devices) {
  DevToolsAgentHost::List remote_targets;
  for (const auto& complete : devices) {
    for (const auto& browser : complete.second->browsers()) {
      for (const auto& page : browser->pages())
        remote_targets.push_back(page->CreateTarget());
    }
  }
  remote_agent_hosts_.swap(remote_targets);
}

void ChromeDevToolsManagerDelegate::UpdateDeviceDiscovery() {
  RemoteLocations remote_locations;
  for (const auto& pair : host_data_) {
    RemoteLocations& locations = pair.second->remote_locations();
    remote_locations.insert(locations.begin(), locations.end());
  }

  bool equals = remote_locations.size() == remote_locations_.size();
  if (equals) {
    RemoteLocations::iterator it1 = remote_locations.begin();
    RemoteLocations::iterator it2 = remote_locations_.begin();
    while (it1 != remote_locations.end()) {
      DCHECK(it2 != remote_locations_.end());
      if (!(*it1).Equals(*it2))
        equals = false;
      ++it1;
      ++it2;
    }
    DCHECK(it2 == remote_locations_.end());
  }

  if (equals)
    return;

  if (remote_locations.empty()) {
    device_discovery_.reset();
    remote_agent_hosts_.clear();
  } else {
    if (!device_manager_)
      device_manager_ = AndroidDeviceManager::Create();

    AndroidDeviceManager::DeviceProviders providers;
    providers.push_back(new TCPDeviceProvider(remote_locations));
    device_manager_->SetDeviceProviders(providers);

    device_discovery_.reset(new DevToolsDeviceDiscovery(device_manager_.get(),
        base::Bind(&ChromeDevToolsManagerDelegate::DevicesAvailable,
                   base::Unretained(this))));
  }
  remote_locations_.swap(remote_locations);
}

std::unique_ptr<base::DictionaryValue>
ChromeDevToolsManagerDelegate::SetRemoteLocations(
    content::DevToolsAgentHost* agent_host,
    int command_id,
    base::DictionaryValue* params) {
  // Could have been created late.
  if (host_data_.find(agent_host) == host_data_.end())
    DevToolsAgentHostAttached(agent_host);

  std::set<net::HostPortPair> tcp_locations;
  base::ListValue* locations;
  if (!params->GetList(kLocationsParam, &locations))
    return DevToolsProtocol::CreateInvalidParamsResponse(command_id,
                                                         kLocationsParam);
  for (const auto& item : *locations) {
    if (!item->IsType(base::Value::Type::DICTIONARY)) {
      return DevToolsProtocol::CreateInvalidParamsResponse(command_id,
                                                           kLocationsParam);
    }
    base::DictionaryValue* dictionary =
        static_cast<base::DictionaryValue*>(item.get());
    std::string host;
    if (!dictionary->GetStringWithoutPathExpansion(kHostParam, &host)) {
      return DevToolsProtocol::CreateInvalidParamsResponse(command_id,
                                                           kLocationsParam);
    }
    int port = 0;
    if (!dictionary->GetIntegerWithoutPathExpansion(kPortParam, &port)) {
      return DevToolsProtocol::CreateInvalidParamsResponse(command_id,
                                                           kLocationsParam);
    }
    tcp_locations.insert(net::HostPortPair(host, port));
  }

  host_data_[agent_host]->set_remote_locations(tcp_locations);
  UpdateDeviceDiscovery();

  std::unique_ptr<base::DictionaryValue> result(
      base::MakeUnique<base::DictionaryValue>());
  return DevToolsProtocol::CreateSuccessResponse(command_id, std::move(result));
}
