// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/webui/register_page_ui.h"

#include <string>

#include "base/callback.h"
#include "base/logging.h"
#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/weak_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/cros/system_library.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/version_loader.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Host page JS API callback names.
const char kJsCallbackGetRegistrationUrl[] = "getRegistrationUrl";
const char kJsCallbackUserInfo[] = "getUserInfo";

// Host page JS API function names.
const wchar_t kJsApiSetRegistrationUrl[] = L"setRegistrationUrl";
const wchar_t kJsApiSetUserInfo[] = L"setUserInfo";
const wchar_t kJsApiSkipRegistration[] = L"skipRegistration";

// Constant value for os_name sent in setUserInfo.
const char kOSName[] = "ChromeOS";

// MachineInfo keys names.
const char kMachineInfoSystemHwqual[] = "hardware_class";
const char kMachineInfoSerialNumber[] = "serial_number";

// Types of network connection.
const char kConnectionEthernet[] = "ethernet";
const char kConnectionWifi[] = "wifi";
const char kConnection3g[] = "3g";
const char kUndefinedValue[] = "undefined";

// Utility function that returns string corresponding to currently active
// connection type |kConnectionEthernet|kConnectionWifi|kConnection3g|.
// If multiple interfaces are connected, result is based on the
// priority Ethernet-Wifi-Cellular.
// If there's no interface that's connected, interface that's in connecting
// state is considered as the active one.
// Otherwise |kUndefinedValue| is returned.
#if defined(OS_CHROMEOS)
static std::string GetConnectionType() {
  if (!chromeos::CrosLibrary::Get()->EnsureLoaded()) {
    LOG(ERROR) << "CrosLibrary is not loaded.";
    return kUndefinedValue;
  }

  chromeos::NetworkLibrary* network_lib =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  if (network_lib->ethernet_connected())
    return kConnectionEthernet;
  else if (network_lib->wifi_connected())
    return kConnectionWifi;
  else if (network_lib->cellular_connected())
    return kConnection3g;
  // Connection might have been lost and is in reconnecting state at this point.
  else if (network_lib->ethernet_connecting())
    return kConnectionEthernet;
  else if (network_lib->wifi_connecting())
    return kConnectionWifi;
  else if (network_lib->cellular_connecting())
    return kConnection3g;
  else
    return kUndefinedValue;
}
#endif

}  // namespace

class RegisterPageUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  RegisterPageUIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
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

  // Init work after Attach.
  void Init();

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui);
  virtual void RegisterMessages();

 private:
  // Handlers for JS WebUI messages.
  void HandleGetRegistrationUrl(const ListValue* args);
  void HandleGetUserInfo(const ListValue* args);

#if defined(OS_CHROMEOS)
  // Callback from chromeos::VersionLoader giving the version.
  void OnVersion(chromeos::VersionLoader::Handle handle, std::string version);
#endif

  // Skips registration logging |error_msg| with log type ERROR.
  void SkipRegistration(const std::string& error_msg);

  // Sends message to host registration page with system/user info data.
  void SendUserInfo();

#if defined(OS_CHROMEOS)
  // Handles asynchronously loading the version.
  chromeos::VersionLoader version_loader_;
#endif

  // Used to request the version.
  CancelableRequestConsumer version_consumer_;

  std::string version_;

  DISALLOW_COPY_AND_ASSIGN(RegisterPageHandler);
};

////////////////////////////////////////////////////////////////////////////////
//
// RegisterPageUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

RegisterPageUIHTMLSource::RegisterPageUIHTMLSource()
    : DataSource(chrome::kChromeUIRegisterPageHost, MessageLoop::current()) {
}

void RegisterPageUIHTMLSource::StartDataRequest(const std::string& path,
                                                bool is_off_the_record,
                                                int request_id) {
  // Make sure that chrome://register is available only during
  // OOBE wizard lifetime and when device has not been registered yet.
#if defined(OS_CHROMEOS)
  if (!WizardController::default_controller() ||
      WizardController::IsDeviceRegistered()) {
    scoped_refptr<RefCountedBytes> empty_bytes(new RefCountedBytes);
    SendResponse(request_id, empty_bytes);
    return;
  }

  static const base::StringPiece register_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_HOST_REGISTRATION_PAGE_HTML));

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(register_html.size());
  std::copy(register_html.begin(),
            register_html.end(),
            html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
#else
  scoped_refptr<RefCountedBytes> empty_bytes(new RefCountedBytes);
  SendResponse(request_id, empty_bytes);
#endif
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

WebUIMessageHandler* RegisterPageHandler::Attach(WebUI* web_ui) {
  return WebUIMessageHandler::Attach(web_ui);
}

void RegisterPageHandler::Init() {
}

void RegisterPageHandler::RegisterMessages() {
#if defined(OS_CHROMEOS)
  web_ui_->RegisterMessageCallback(kJsCallbackGetRegistrationUrl,
      NewCallback(this, &RegisterPageHandler::HandleGetRegistrationUrl));
  web_ui_->RegisterMessageCallback(kJsCallbackUserInfo,
      NewCallback(this, &RegisterPageHandler::HandleGetUserInfo));
#endif
}

void RegisterPageHandler::HandleGetRegistrationUrl(const ListValue* args) {
#if defined(OS_CHROMEOS)
  if (WizardController::default_controller() &&
      WizardController::default_controller()->GetCustomization()) {
    const std::string& url = WizardController::default_controller()->
        GetCustomization()->registration_url();
    VLOG(1) << "Loading registration form with URL: " << url;
    GURL register_url(url);
    if (!register_url.is_valid()) {
      SkipRegistration("Registration URL defined in manifest is invalid.");
      return;
    }
    StringValue url_value(url);
    web_ui_->CallJavascriptFunction(kJsApiSetRegistrationUrl, url_value);
  } else {
    SkipRegistration("Startup manifest not defined.");
  }
#endif
}

void RegisterPageHandler::HandleGetUserInfo(const ListValue* args) {
#if defined(OS_CHROMEOS)
  if (chromeos::CrosLibrary::Get()->EnsureLoaded()) {
     version_loader_.GetVersion(
         &version_consumer_,
         NewCallback(this,
                     &RegisterPageHandler::OnVersion),
         chromeos::VersionLoader::VERSION_FULL);
  } else {
    SkipRegistration("CrosLibrary is not loaded.");
  }
#endif
}

#if defined(OS_CHROMEOS)
void RegisterPageHandler::OnVersion(chromeos::VersionLoader::Handle handle,
                                    std::string version) {
  version_ = version;
  SendUserInfo();
}
#endif

void RegisterPageHandler::SkipRegistration(const std::string& error_msg) {
#if defined(OS_CHROMEOS)
  LOG(ERROR) << error_msg;
  if (WizardController::default_controller())
    WizardController::default_controller()->SkipRegistration();
  else
    web_ui_->CallJavascriptFunction(kJsApiSkipRegistration);
#endif
}

void RegisterPageHandler::SendUserInfo() {
#if defined(OS_CHROMEOS)
  DictionaryValue value;

  std::string system_sku;
  if (WizardController::default_controller() &&
      WizardController::default_controller()->GetCustomization()) {
    system_sku = WizardController::default_controller()->
        GetCustomization()->product_sku();
  } else {
    SkipRegistration("Startup manifest not defined.");
    return;
  }

  chromeos::SystemLibrary* sys_lib =
      chromeos::CrosLibrary::Get()->GetSystemLibrary();

  // Required info.
  std::string system_hwqual;
  std::string serial_number;
  if (!sys_lib->GetMachineStatistic(kMachineInfoSystemHwqual, &system_hwqual) ||
      !sys_lib->GetMachineStatistic(kMachineInfoSerialNumber, &serial_number)) {
    SkipRegistration("Failed to get required machine info.");
    return;
  }
  value.SetString("system_hwqual", system_hwqual);
  value.SetString("system_sku", system_sku);
  value.SetString("system_serial", serial_number);
  value.SetString("os_language", g_browser_process->GetApplicationLocale());
  value.SetString("os_name", kOSName);
  value.SetString("os_version", version_);
  value.SetString("os_connection", GetConnectionType());
  value.SetString("user_email", "");

  // Optional info.
  value.SetString("user_first_name", "");
  value.SetString("user_last_name", "");

  web_ui_->CallJavascriptFunction(kJsApiSetUserInfo, value);
#endif
}

////////////////////////////////////////////////////////////////////////////////
//
// RegisterPageUI
//
////////////////////////////////////////////////////////////////////////////////

RegisterPageUI::RegisterPageUI(TabContents* contents) : WebUI(contents){
  RegisterPageHandler* handler = new RegisterPageHandler();
  AddMessageHandler((handler)->Attach(this));
  handler->Init();
  RegisterPageUIHTMLSource* html_source = new RegisterPageUIHTMLSource();

  // Set up the chrome://register/ source.
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);
}
