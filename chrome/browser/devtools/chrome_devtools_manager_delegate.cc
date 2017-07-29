// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"

#include <utility>

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
#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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

namespace {

char kLocationsParam[] = "locations";
char kHostParam[] = "host";
char kPortParam[] = "port";

BrowserWindow* GetBrowserWindow(int window_id) {
  for (auto* b : *BrowserList::GetInstance()) {
    if (b->session_id().id() == window_id)
      return b->window();
  }
  return nullptr;
}

// Get the bounds and state of the browser window. The bounds is for the
// restored window when the window is minimized. Otherwise, it is for the actual
// window.
std::unique_ptr<base::DictionaryValue> GetBounds(BrowserWindow* window) {
  gfx::Rect bounds;
  if (window->IsMinimized())
    bounds = window->GetRestoredBounds();
  else
    bounds = window->GetBounds();

  auto bounds_object = base::MakeUnique<base::DictionaryValue>();

  bounds_object->SetInteger("left", bounds.x());
  bounds_object->SetInteger("top", bounds.y());
  bounds_object->SetInteger("width", bounds.width());
  bounds_object->SetInteger("height", bounds.height());

  std::string window_state = "normal";
  if (window->IsMinimized())
    window_state = "minimized";
  if (window->IsMaximized())
    window_state = "maximized";
  if (window->IsFullscreen())
    window_state = "fullscreen";
  bounds_object->SetString("windowState", window_state);

  return bounds_object;
}

bool GetExtensionInfo(content::WebContents* wc,
                      std::string* name,
                      std::string* type) {
  Profile* profile = Profile::FromBrowserContext(wc->GetBrowserContext());
  if (!profile)
    return false;
  const extensions::Extension* extension =
      extensions::ProcessManager::Get(profile)->GetExtensionForWebContents(wc);
  if (!extension)
    return false;
  extensions::ExtensionHost* extension_host =
      extensions::ProcessManager::Get(profile)->GetBackgroundHostForExtension(
          extension->id());
  if (extension_host && extension_host->host_contents() == wc) {
    *name = extension->name();
    *type = ChromeDevToolsManagerDelegate::kTypeBackgroundPage;
    return true;
  } else if (extension->is_hosted_app() ||
             extension->is_legacy_packaged_app() ||
             extension->is_platform_app()) {
    *name = extension->name();
    *type = ChromeDevToolsManagerDelegate::kTypeApp;
    return true;
  }
  return false;
}

void ToggleAdBlocking(bool enabled, content::DevToolsAgentHost* agent_host) {
  if (content::WebContents* web_contents = agent_host->GetWebContents()) {
    if (auto* client =
            ChromeSubresourceFilterClient::FromWebContents(web_contents)) {
      client->ToggleForceActivationInCurrentWebContents(enabled);
    }
  }
}

}  // namespace

// static
std::unique_ptr<base::DictionaryValue>
ChromeDevToolsManagerDelegate::GetWindowForTarget(
    int id,
    base::DictionaryValue* params) {
  std::string target_id;
  if (!params->GetString("targetId", &target_id))
    return DevToolsProtocol::CreateInvalidParamsResponse(id, "targetId");

  Browser* browser = nullptr;
  scoped_refptr<DevToolsAgentHost> host =
      DevToolsAgentHost::GetForId(target_id);
  if (!host)
    return DevToolsProtocol::CreateErrorResponse(id, "No target with given id");
  content::WebContents* web_contents = host->GetWebContents();
  if (!web_contents) {
    return DevToolsProtocol::CreateErrorResponse(
        id, "No web contents in the target");
  }
  for (auto* b : *BrowserList::GetInstance()) {
    int tab_index = b->tab_strip_model()->GetIndexOfWebContents(web_contents);
    if (tab_index != TabStripModel::kNoTab)
      browser = b;
  }
  if (!browser) {
    return DevToolsProtocol::CreateErrorResponse(id,
                                                 "Browser window not found");
  }

  auto result = base::MakeUnique<base::DictionaryValue>();
  result->SetInteger("windowId", browser->session_id().id());
  result->Set("bounds", GetBounds(browser->window()));
  return DevToolsProtocol::CreateSuccessResponse(id, std::move(result));
}

// static
std::unique_ptr<base::DictionaryValue>
ChromeDevToolsManagerDelegate::GetWindowBounds(int id,
                                               base::DictionaryValue* params) {
  int window_id;
  if (!params->GetInteger("windowId", &window_id))
    return DevToolsProtocol::CreateInvalidParamsResponse(id, "windowId");
  BrowserWindow* window = GetBrowserWindow(window_id);
  if (!window) {
    return DevToolsProtocol::CreateErrorResponse(id,
                                                 "Browser window not found");
  }

  auto result = base::MakeUnique<base::DictionaryValue>();
  result->Set("bounds", GetBounds(window));
  return DevToolsProtocol::CreateSuccessResponse(id, std::move(result));
}

// static
std::unique_ptr<base::DictionaryValue>
ChromeDevToolsManagerDelegate::SetWindowBounds(int id,
                                               base::DictionaryValue* params) {
  int window_id;
  if (!params->GetInteger("windowId", &window_id))
    return DevToolsProtocol::CreateInvalidParamsResponse(id, "windowId");
  BrowserWindow* window = GetBrowserWindow(window_id);
  if (!window) {
    return DevToolsProtocol::CreateErrorResponse(id,
                                                 "Browser window not found");
  }

  const base::Value* value = nullptr;
  const base::DictionaryValue* bounds_dict = nullptr;
  if (!params->Get("bounds", &value) || !value->GetAsDictionary(&bounds_dict))
    return DevToolsProtocol::CreateInvalidParamsResponse(id, "bounds");

  std::string window_state;
  if (!bounds_dict->GetString("windowState", &window_state))
    window_state = "normal";
  else if (window_state != "normal" && window_state != "minimized" &&
           window_state != "maximized" && window_state != "fullscreen")
    return DevToolsProtocol::CreateInvalidParamsResponse(id, "windowState");

  // Compute updated bounds when window state is normal.
  bool set_bounds = false;
  gfx::Rect bounds = window->GetBounds();
  int left, top, width, height;
  if (bounds_dict->GetInteger("left", &left)) {
    bounds.set_x(left);
    set_bounds = true;
  }
  if (bounds_dict->GetInteger("top", &top)) {
    bounds.set_y(top);
    set_bounds = true;
  }
  if (bounds_dict->GetInteger("width", &width)) {
    if (width < 0)
      return DevToolsProtocol::CreateInvalidParamsResponse(id, "width");
    bounds.set_width(width);
    set_bounds = true;
  }
  if (bounds_dict->GetInteger("height", &height)) {
    if (height < 0)
      return DevToolsProtocol::CreateInvalidParamsResponse(id, "height");
    bounds.set_height(height);
    set_bounds = true;
  }

  if (set_bounds && window_state != "normal") {
    return DevToolsProtocol::CreateErrorResponse(
        id,
        "The 'minimized', 'maximized' and 'fullscreen' states cannot be "
        "combined with 'left', 'top', 'width' or 'height'");
  }

  if (set_bounds && (window->IsMinimized() || window->IsMaximized() ||
                     window->IsFullscreen())) {
    return DevToolsProtocol::CreateErrorResponse(
        id,
        "To resize minimized/maximized/fullscreen window, restore it to normal "
        "state first.");
  }

  if (window_state == "fullscreen") {
    if (window->IsMinimized()) {
      return DevToolsProtocol::CreateErrorResponse(id,
                                                   "To make minimized window "
                                                   "fullscreen, restore it to "
                                                   "normal state first.");
    }
    window->GetExclusiveAccessContext()->EnterFullscreen(
        GURL(), EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE);
  }

  if (window_state == "maximized") {
    if (window->IsMinimized() || window->IsFullscreen()) {
      return DevToolsProtocol::CreateErrorResponse(
          id,
          "To maximize a minimized or fullscreen window, restore it to normal "
          "state first.");
    }
    window->Maximize();
  }

  if (window_state == "minimized") {
    if (window->IsFullscreen()) {
      return DevToolsProtocol::CreateErrorResponse(
          id,
          "To minimize a fullscreen window, restore it to normal "
          "state first.");
    }
    window->Minimize();
  }

  if (window_state == "normal") {
    if (window->IsFullscreen()) {
      window->GetExclusiveAccessContext()->ExitFullscreen();
    } else if (window->IsMinimized()) {
      window->Show();
    } else if (window->IsMaximized()) {
      window->Restore();
    } else if (set_bounds) {
      window->SetBounds(bounds);
    }
  }

  return DevToolsProtocol::CreateSuccessResponse(id, nullptr);
}

std::unique_ptr<base::DictionaryValue>
ChromeDevToolsManagerDelegate::SetAdBlockingEnabled(
    content::DevToolsAgentHost* agent_host,
    int id,
    base::DictionaryValue* params) {
  if (!page_enable_)
    return DevToolsProtocol::CreateErrorResponse(id, "Page domain is disabled");
  bool enabled = false;
  params->GetBoolean("enabled", &enabled);
  ToggleAdBlocking(enabled, agent_host);
  return DevToolsProtocol::CreateSuccessResponse(id, nullptr);
}

void ChromeDevToolsManagerDelegate::TogglePageEnable(
    bool enable,
    content::DevToolsAgentHost* agent_host) {
  page_enable_ = enable;
  if (!page_enable_)
    ToggleAdBlocking(false /* enable */, agent_host);
}

std::unique_ptr<base::DictionaryValue>
ChromeDevToolsManagerDelegate::HandleBrowserCommand(
    int id,
    std::string method,
    base::DictionaryValue* params) {
  if (method == chrome::devtools::Browser::getWindowForTarget::kName)
    return GetWindowForTarget(id, params);
  if (method == chrome::devtools::Browser::getWindowBounds::kName)
    return GetWindowBounds(id, params);
  if (method == chrome::devtools::Browser::setWindowBounds::kName)
    return SetWindowBounds(id, params);
  return nullptr;
}

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

  // Do not actually handle the enable/disable commands, just keep track of the
  // enable state.
  if (method == chrome::devtools::Page::enable::kName)
    TogglePageEnable(true /* enable */, agent_host);
  if (method == chrome::devtools::Page::disable::kName)
    TogglePageEnable(false /* enable */, agent_host);

  if (agent_host->GetType() == DevToolsAgentHost::kTypeBrowser &&
      method.find("Browser.") == 0)
    return HandleBrowserCommand(id, method, params).release();

  if (method == chrome::devtools::Page::setAdBlockingEnabled::kName)
    return SetAdBlockingEnabled(agent_host, id, params).release();

  if (method == chrome::devtools::Target::setRemoteLocations::kName)
    return SetRemoteLocations(agent_host, id, params).release();

  return network_protocol_handler_->HandleCommand(agent_host, command_dict);
}

std::string ChromeDevToolsManagerDelegate::GetTargetType(
    content::WebContents* web_contents) {
  for (TabContentsIterator it; !it.done(); it.Next()) {
    if (*it == web_contents)
      return DevToolsAgentHost::kTypePage;
  }

  std::string extension_name;
  std::string extension_type;
  if (!GetExtensionInfo(web_contents, &extension_name, &extension_type))
    return DevToolsAgentHost::kTypeOther;
  return extension_type;
}

std::string ChromeDevToolsManagerDelegate::GetTargetTitle(
    content::WebContents* web_contents) {
  std::string extension_name;
  std::string extension_type;
  if (!GetExtensionInfo(web_contents, &extension_name, &extension_type))
    return std::string();
  return extension_name;
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
  ToggleAdBlocking(false /* enable */, agent_host);

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
    if (!item.IsType(base::Value::Type::DICTIONARY)) {
      return DevToolsProtocol::CreateInvalidParamsResponse(command_id,
                                                           kLocationsParam);
    }
    const base::DictionaryValue* dictionary =
        static_cast<const base::DictionaryValue*>(&item);
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

  return DevToolsProtocol::CreateSuccessResponse(command_id, nullptr);
}
