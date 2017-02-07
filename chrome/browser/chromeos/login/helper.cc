// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/helper.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_util.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {

namespace {

// Gets the WebContents instance of current login display. If there is none,
// returns nullptr.
content::WebContents* GetLoginWebContents() {
  LoginDisplayHost* host = LoginDisplayHost::default_host();
  if (!host || !host->GetWebUILoginView())
    return nullptr;

  return host->GetWebUILoginView()->GetWebContents();
}

// Callback used by GetPartition below to return the first guest contents with a
// matching partition name.
bool FindGuestByPartitionName(const std::string& partition_name,
                              content::WebContents** out_guest_contents,
                              content::WebContents* guest_contents) {
  std::string domain;
  std::string name;
  bool in_memory;
  extensions::WebViewGuest::GetGuestPartitionConfigForSite(
      guest_contents->GetSiteInstance()->GetSiteURL(), &domain, &name,
      &in_memory);
  if (partition_name != name)
    return false;

  *out_guest_contents = guest_contents;
  return true;
}

// Gets the storage partition of guest contents of a given embedder.
// If a name is given, returns the partition associated with the name.
// Otherwise, returns the default shared in-memory partition. Returns nullptr if
// a matching partition could not be found.
content::StoragePartition* GetPartition(content::WebContents* embedder,
                                        const std::string& partition_name) {
  guest_view::GuestViewManager* manager =
      guest_view::GuestViewManager::FromBrowserContext(
          embedder->GetBrowserContext());
  if (!manager)
    return nullptr;

  content::WebContents* guest_contents = nullptr;
  manager->ForEachGuest(embedder, base::Bind(&FindGuestByPartitionName,
                                             partition_name, &guest_contents));

  return guest_contents ? content::BrowserContext::GetStoragePartition(
                              guest_contents->GetBrowserContext(),
                              guest_contents->GetSiteInstance())
                        : nullptr;
}

base::ScopedFD GetDataReadPipe(const std::string& data) {
  int pipe_fds[2];
  if (!base::CreateLocalNonBlockingPipe(pipe_fds)) {
    DLOG(ERROR) << "Failed to create pipe";
    return base::ScopedFD();
  }
  base::ScopedFD pipe_read_end(pipe_fds[0]);
  base::ScopedFD pipe_write_end(pipe_fds[1]);

  if (!base::WriteFileDescriptor(pipe_write_end.get(), data.c_str(),
                                 data.size())) {
    DLOG(ERROR) << "Failed to write to pipe";
    return base::ScopedFD();
  }
  return pipe_read_end;
}

}  // namespace

gfx::Rect CalculateScreenBounds(const gfx::Size& size) {
  gfx::Rect bounds = display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  if (!size.IsEmpty()) {
    int horizontal_diff = bounds.width() - size.width();
    int vertical_diff = bounds.height() - size.height();
    bounds.Inset(horizontal_diff / 2, vertical_diff / 2);
  }
  return bounds;
}

int GetCurrentUserImageSize() {
  // The biggest size that the profile picture is displayed at is currently
  // 220px, used for the big preview on OOBE and Change Picture options page.
  static const int kBaseUserImageSize = 220;
  float scale_factor = display::Display::GetForcedDeviceScaleFactor();
  if (scale_factor > 1.0f)
    return static_cast<int>(scale_factor * kBaseUserImageSize);
  return kBaseUserImageSize * gfx::ImageSkia::GetMaxSupportedScale();
}

namespace login {

NetworkStateHelper::NetworkStateHelper() {}
NetworkStateHelper::~NetworkStateHelper() {}

base::string16 NetworkStateHelper::GetCurrentNetworkName() const {
  NetworkStateHandler* nsh = NetworkHandler::Get()->network_state_handler();
  const NetworkState* network =
      nsh->ConnectedNetworkByType(NetworkTypePattern::NonVirtual());
  if (network) {
    if (network->Matches(NetworkTypePattern::Ethernet()))
      return l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
    return base::UTF8ToUTF16(network->name());
  }

  network = nsh->ConnectingNetworkByType(NetworkTypePattern::NonVirtual());
  if (network) {
    if (network->Matches(NetworkTypePattern::Ethernet()))
      return l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
    return base::UTF8ToUTF16(network->name());
  }
  return base::string16();
}

void NetworkStateHelper::GetConnectedWifiNetwork(std::string* out_onc_spec) {
  const NetworkState* network_state =
      NetworkHandler::Get()->network_state_handler()->ConnectedNetworkByType(
          NetworkTypePattern::WiFi());

  if (!network_state)
    return;

  std::unique_ptr<base::DictionaryValue> current_onc =
      network_util::TranslateNetworkStateToONC(network_state);
  std::string security;
  current_onc->GetString(
      onc::network_config::WifiProperty(onc::wifi::kSecurity), &security);
  if (security != onc::wifi::kSecurityNone)
    return;

  const std::string hex_ssid = network_state->GetHexSsid();

  std::unique_ptr<base::DictionaryValue> copied_onc(
      new base::DictionaryValue());
  copied_onc->Set(onc::toplevel_config::kType,
                  new base::StringValue(onc::network_type::kWiFi));
  copied_onc->Set(onc::network_config::WifiProperty(onc::wifi::kHexSSID),
                  new base::StringValue(hex_ssid));
  copied_onc->Set(onc::network_config::WifiProperty(onc::wifi::kSecurity),
                  new base::StringValue(security));
  base::JSONWriter::Write(*copied_onc.get(), out_onc_spec);
}

void NetworkStateHelper::CreateAndConnectNetworkFromOnc(
    const std::string& onc_spec,
    const base::Closure& success_callback,
    const base::Closure& error_callback) const {
  std::string error;
  std::unique_ptr<base::Value> root = base::JSONReader::ReadAndReturnError(
      onc_spec, base::JSON_ALLOW_TRAILING_COMMAS, nullptr, &error);

  base::DictionaryValue* toplevel_onc = nullptr;
  if (!root || !root->GetAsDictionary(&toplevel_onc)) {
    LOG(ERROR) << "Invalid JSON Dictionary: " << error;
    error_callback.Run();
    return;
  }

  NetworkHandler::Get()
      ->managed_network_configuration_handler()
      ->CreateConfiguration(
          "", *toplevel_onc,
          base::Bind(&NetworkStateHelper::OnCreateConfiguration,
                     base::Unretained(this), success_callback, error_callback),
          base::Bind(&NetworkStateHelper::OnCreateOrConnectNetworkFailed,
                     base::Unretained(this), error_callback));
}

bool NetworkStateHelper::IsConnected() const {
  chromeos::NetworkStateHandler* nsh =
      chromeos::NetworkHandler::Get()->network_state_handler();
  return nsh->ConnectedNetworkByType(chromeos::NetworkTypePattern::Default()) !=
         nullptr;
}

bool NetworkStateHelper::IsConnecting() const {
  chromeos::NetworkStateHandler* nsh =
      chromeos::NetworkHandler::Get()->network_state_handler();
  return nsh->ConnectingNetworkByType(
      chromeos::NetworkTypePattern::Default()) != nullptr;
}

void NetworkStateHelper::OnCreateConfiguration(
    const base::Closure& success_callback,
    const base::Closure& error_callback,
    const std::string& service_path,
    const std::string& guid) const {
  // Connect to the network.
  NetworkHandler::Get()->network_connection_handler()->ConnectToNetwork(
      service_path, success_callback,
      base::Bind(&NetworkStateHelper::OnCreateOrConnectNetworkFailed,
                 base::Unretained(this), error_callback),
      false);
}

void NetworkStateHelper::OnCreateOrConnectNetworkFailed(
    const base::Closure& error_callback,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) const {
  LOG(ERROR) << "Failed to create or connect to network: " << error_name;
  error_callback.Run();
}

content::StoragePartition* GetSigninPartition() {
  content::WebContents* embedder = GetLoginWebContents();
  if (!embedder)
    return nullptr;

  // Note the partition name must match the sign-in webview used. For now,
  // this is the default unnamed, shared, in-memory partition.
  return GetPartition(embedder, std::string());
}

net::URLRequestContextGetter* GetSigninContext() {
  content::StoragePartition* signin_partition = GetSigninPartition();

  // Special case for unit tests. There's no LoginDisplayHost thus no
  // webview instance. See http://crbug.com/477402
  if (!signin_partition && !LoginDisplayHost::default_host())
    return ProfileHelper::GetSigninProfile()->GetRequestContext();

  if (!signin_partition)
    return nullptr;

  return signin_partition->GetURLRequestContext();
}

void GetPipeReadEnd(const std::string& data,
                    const OnPipeReadyCallback& callback) {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, base::TaskTraits().MayBlock().WithPriority(
                     base::TaskPriority::BACKGROUND),
      base::Bind(&GetDataReadPipe, data), callback);
}

}  // namespace login

}  // namespace chromeos
