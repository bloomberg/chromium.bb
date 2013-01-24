// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/register_page_ui.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/chromeos/chromeos_version.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chrome/browser/chromeos/version_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

// Host page JS API callback names.
const char kJsCallbackGetRegistrationUrl[] = "getRegistrationUrl";
const char kJsCallbackUserInfo[] = "getUserInfo";

// Host page JS API function names.
const char kJsApiSetRegistrationUrl[] = "setRegistrationUrl";
const char kJsApiSetUserInfo[] = "setUserInfo";
const char kJsApiSkipRegistration[] = "skipRegistration";

// Constant value for os_name sent in setUserInfo.
const char kOSName[] = "ChromeOS";

// MachineInfo keys names.
const char kMachineInfoSystemHwqual[] = "hardware_class";
const char kMachineInfoSerialNumber[] = "serial_number";

// Types of network connection.
const char kConnectionEthernet[] = "ethernet";
const char kConnectionWifi[] = "wifi";
const char kConnectionWimax[] = "wimax";
const char kConnection3g[] = "3g";
const char kUndefinedValue[] = "undefined";

// Utility function that returns string corresponding to currently active
// connection type |kConnectionEthernet|kConnectionWifi|kConnection3g|.
// If multiple interfaces are connected, result is based on the
// priority Ethernet-Wifi-Cellular.
// If there's no interface that's connected, interface that's in connecting
// state is considered as the active one.
// Otherwise |kUndefinedValue| is returned.
static std::string GetConnectionType() {
  chromeos::NetworkLibrary* network_lib =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  if (network_lib->ethernet_connected())
    return kConnectionEthernet;
  else if (network_lib->wifi_connected())
    return kConnectionWifi;
  else if (network_lib->cellular_connected())
    return kConnection3g;
  else if (network_lib->wimax_connected())
    return kConnectionWimax;
  // Connection might have been lost and is in reconnecting state at this point.
  else if (network_lib->ethernet_connecting())
    return kConnectionEthernet;
  else if (network_lib->wifi_connecting())
    return kConnectionWifi;
  else if (network_lib->cellular_connecting())
    return kConnection3g;
  else if (network_lib->wimax_connecting())
    return kConnectionWimax;
  else
    return kUndefinedValue;
}

}  // namespace

class RegisterPageUIHTMLSource : public content::URLDataSource {
 public:
  RegisterPageUIHTMLSource();

  // content::URLDataSource implementation.
  virtual std::string GetSource() OVERRIDE;
  virtual void StartDataRequest(
      const std::string& path,
      bool is_incognito,
      const content::URLDataSource::GotDataCallback& callback);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }
  virtual bool ShouldAddContentSecurityPolicy() const OVERRIDE {
    return false;
  }

 private:
  ~RegisterPageUIHTMLSource() {}

  DISALLOW_COPY_AND_ASSIGN(RegisterPageUIHTMLSource);
};

// The handler for Javascript messages related to the "register" view.
class RegisterPageHandler : public WebUIMessageHandler,
                            public base::SupportsWeakPtr<RegisterPageHandler> {
 public:
  RegisterPageHandler();
  virtual ~RegisterPageHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Handlers for JS WebUI messages.
  void HandleGetRegistrationUrl(const ListValue* args);
  void HandleGetUserInfo(const ListValue* args);

  // Callback from chromeos::VersionLoader giving the version.
  void OnVersion(const std::string& version);

  // Skips registration logging |error_msg| with log type ERROR.
  void SkipRegistration(const std::string& error_msg);

  // Sends message to host registration page with system/user info data.
  void SendUserInfo();

  // Handles asynchronously loading the version.
  chromeos::VersionLoader version_loader_;

  // Used to request the version.
  CancelableTaskTracker tracker_;

  std::string version_;

  DISALLOW_COPY_AND_ASSIGN(RegisterPageHandler);
};

////////////////////////////////////////////////////////////////////////////////
//
// RegisterPageUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

RegisterPageUIHTMLSource::RegisterPageUIHTMLSource() {
}

std::string RegisterPageUIHTMLSource::GetSource() {
  return chrome::kChromeUIRegisterPageHost;
}

void RegisterPageUIHTMLSource::StartDataRequest(
    const std::string& path,
    bool is_incognito,
    const content::URLDataSource::GotDataCallback& callback) {
  // Make sure that chrome://register is available only during
  // OOBE wizard lifetime and when device has not been registered yet.
  if (!chromeos::WizardController::default_controller() ||
      chromeos::WizardController::IsDeviceRegistered()) {
    scoped_refptr<base::RefCountedBytes> empty_bytes(new base::RefCountedBytes);
    callback.Run(empty_bytes);
    return;
  }

  scoped_refptr<base::RefCountedMemory> html_bytes(
      ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
          IDR_HOST_REGISTRATION_PAGE_HTML));

  callback.Run(html_bytes);
}

////////////////////////////////////////////////////////////////////////////////
//
// RegisterPageHandler
//
////////////////////////////////////////////////////////////////////////////////
RegisterPageHandler::RegisterPageHandler() {
}

RegisterPageHandler::~RegisterPageHandler() {
}

void RegisterPageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(kJsCallbackGetRegistrationUrl,
      base::Bind(&RegisterPageHandler::HandleGetRegistrationUrl,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kJsCallbackUserInfo,
      base::Bind(&RegisterPageHandler::HandleGetUserInfo,
                 base::Unretained(this)));
}

void RegisterPageHandler::HandleGetRegistrationUrl(const ListValue* args) {
  chromeos::StartupCustomizationDocument* customization =
    chromeos::StartupCustomizationDocument::GetInstance();
  if (chromeos::WizardController::default_controller() &&
      customization->IsReady()) {
    const std::string& url = customization->registration_url();
    VLOG(1) << "Loading registration form with URL: " << url;
    GURL register_url(url);
    if (!register_url.is_valid()) {
      SkipRegistration("Registration URL defined in manifest is invalid.");
      return;
    }
    StringValue url_value(url);
    web_ui()->CallJavascriptFunction(kJsApiSetRegistrationUrl, url_value);
  } else {
    SkipRegistration("Startup manifest not defined.");
  }
}

void RegisterPageHandler::HandleGetUserInfo(const ListValue* args) {
  if (base::chromeos::IsRunningOnChromeOS()) {
     version_loader_.GetVersion(
         chromeos::VersionLoader::VERSION_FULL,
         base::Bind(&RegisterPageHandler::OnVersion, base::Unretained(this)),
         &tracker_);
  } else {
    SkipRegistration("Not running on ChromeOS.");
  }
}

void RegisterPageHandler::OnVersion(const std::string& version) {
  version_ = version;
  SendUserInfo();
}

void RegisterPageHandler::SkipRegistration(const std::string& error_msg) {
  LOG(ERROR) << error_msg;
  if (chromeos::WizardController::default_controller())
    chromeos::WizardController::default_controller()->SkipRegistration();
  else
    web_ui()->CallJavascriptFunction(kJsApiSkipRegistration);
}

void RegisterPageHandler::SendUserInfo() {
  DictionaryValue value;

  chromeos::system::StatisticsProvider * provider =
      chromeos::system::StatisticsProvider::GetInstance();

  // Required info.
  std::string system_hwqual;
  std::string serial_number;
  if (!provider->GetMachineStatistic(kMachineInfoSystemHwqual,
                                     &system_hwqual) ||
      !provider->GetMachineStatistic(kMachineInfoSerialNumber,
                                     &serial_number)) {
    SkipRegistration("Failed to get required machine info.");
    return;
  }
  value.SetString("system_hwqual", system_hwqual);
  value.SetString("system_serial", serial_number);
  value.SetString("os_language", g_browser_process->GetApplicationLocale());
  value.SetString("os_name", kOSName);
  value.SetString("os_version", version_);
  value.SetString("os_connection", GetConnectionType());
  value.SetString("user_email", "");

  // Optional info.
  value.SetString("user_first_name", "");
  value.SetString("user_last_name", "");

  web_ui()->CallJavascriptFunction(kJsApiSetUserInfo, value);
}

////////////////////////////////////////////////////////////////////////////////
//
// RegisterPageUI
//
////////////////////////////////////////////////////////////////////////////////

RegisterPageUI::RegisterPageUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  RegisterPageHandler* handler = new RegisterPageHandler();
  web_ui->AddMessageHandler(handler);
  RegisterPageUIHTMLSource* html_source = new RegisterPageUIHTMLSource();

  // Set up the chrome://register/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSource(profile, html_source);
}
