// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/mobile_setup_ui.h"

#include <algorithm>
#include <map>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/mobile/mobile_activator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

using chromeos::MobileActivator;
using chromeos::NetworkHandler;
using chromeos::NetworkState;
using content::BrowserThread;
using content::RenderViewHost;
using content::WebContents;
using content::WebUIMessageHandler;

namespace {

// Host page JS API function names.
const char kJsApiStartActivation[] = "startActivation";
const char kJsApiSetTransactionStatus[] = "setTransactionStatus";
const char kJsApiPaymentPortalLoad[] = "paymentPortalLoad";
const char kJsGetDeviceInfo[] = "getDeviceInfo";
const char kJsApiResultOK[] = "ok";

const char kJsDeviceStatusChangedCallback[] =
    "mobile.MobileSetup.deviceStateChanged";
const char kJsPortalFrameLoadFailedCallback[] =
    "mobile.MobileSetup.portalFrameLoadError";
const char kJsPortalFrameLoadCompletedCallback[] =
    "mobile.MobileSetup.portalFrameLoadCompleted";
const char kJsGetDeviceInfoCallback[] =
    "mobile.MobileSetupPortal.onGotDeviceInfo";
const char kJsConnectivityChangedCallback[] =
    "mobile.MobileSetupPortal.onConnectivityChanged";

void DataRequestFailed(
    const std::string& service_path,
    const content::URLDataSource::GotDataCallback& callback) {
  NET_LOG_ERROR("Data Request Failed for Mobile Setup", service_path);
  scoped_refptr<base::RefCountedBytes> html_bytes(new base::RefCountedBytes);
  callback.Run(html_bytes.get());
}

// Converts the network properties into a JS object.
void GetDeviceInfo(const base::DictionaryValue& properties,
                   base::DictionaryValue* value) {
  std::string name;
  properties.GetStringWithoutPathExpansion(shill::kNameProperty, &name);
  bool activate_over_non_cellular_networks = false;
  properties.GetBooleanWithoutPathExpansion(
      shill::kActivateOverNonCellularNetworkProperty,
      &activate_over_non_cellular_networks);
  const base::DictionaryValue* payment_dict;
  std::string payment_url, post_method, post_data;
  if (properties.GetDictionaryWithoutPathExpansion(
          shill::kPaymentPortalProperty, &payment_dict)) {
    payment_dict->GetStringWithoutPathExpansion(
        shill::kPaymentPortalURL, &payment_url);
    payment_dict->GetStringWithoutPathExpansion(
        shill::kPaymentPortalMethod, &post_method);
    payment_dict->GetStringWithoutPathExpansion(
        shill::kPaymentPortalPostData, &post_data);
  }

  value->SetBoolean("activate_over_non_cellular_network",
                    activate_over_non_cellular_networks);
  value->SetString("carrier", name);
  value->SetString("payment_url", payment_url);
  if (LowerCaseEqualsASCII(post_method, "post") && !post_data.empty())
    value->SetString("post_data", post_data);

  // Use the cached DeviceState properties.
  std::string device_path;
  if (!properties.GetStringWithoutPathExpansion(
          shill::kDeviceProperty, &device_path) ||
      device_path.empty()) {
    return;
  }
  const chromeos::DeviceState* device =
      NetworkHandler::Get()->network_state_handler()->GetDeviceState(
          device_path);
  if (!device)
    return;

  value->SetString("MEID", device->meid());
  value->SetString("IMEI", device->imei());
  value->SetString("MDN", device->mdn());
}

void SetActivationStateAndError(MobileActivator::PlanActivationState state,
                                const std::string& error_description,
                                base::DictionaryValue* value) {
  value->SetInteger("state", state);
  if (!error_description.empty())
    value->SetString("error", error_description);
}

}  // namespace

class MobileSetupUIHTMLSource : public content::URLDataSource {
 public:
  MobileSetupUIHTMLSource();

  // content::URLDataSource implementation.
  virtual std::string GetSource() const OVERRIDE;
  virtual void StartDataRequest(
      const std::string& path,
      int render_process_id,
      int render_frame_id,
      const content::URLDataSource::GotDataCallback& callback) OVERRIDE;
  virtual std::string GetMimeType(const std::string&) const OVERRIDE {
    return "text/html";
  }
  virtual bool ShouldAddContentSecurityPolicy() const OVERRIDE {
    return false;
  }

 private:
  virtual ~MobileSetupUIHTMLSource() {}

  void GetPropertiesAndStartDataRequest(
      const content::URLDataSource::GotDataCallback& callback,
      const std::string& service_path,
      const base::DictionaryValue& properties);
  void GetPropertiesFailure(
      const content::URLDataSource::GotDataCallback& callback,
      const std::string& service_path,
      const std::string& error_name,
      scoped_ptr<base::DictionaryValue> error_data);

  base::WeakPtrFactory<MobileSetupUIHTMLSource> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MobileSetupUIHTMLSource);
};

// The handler for Javascript messages related to the "register" view.
class MobileSetupHandler
  : public WebUIMessageHandler,
    public MobileActivator::Observer,
    public chromeos::NetworkStateHandlerObserver,
    public base::SupportsWeakPtr<MobileSetupHandler> {
 public:
  MobileSetupHandler();
  virtual ~MobileSetupHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  enum Type {
    TYPE_UNDETERMINED,
    // The network is not yet activated, and the webui is in activation flow.
    TYPE_ACTIVATION,
    // The network is activated, the webui displays network portal.
    TYPE_PORTAL,
    // Same as TYPE_PORTAL, but the network technology is LTE. The webui is
    // additionally aware of network manager state and whether the portal can be
    // reached.
    TYPE_PORTAL_LTE
  };

  // MobileActivator::Observer.
  virtual void OnActivationStateChanged(
      const NetworkState* network,
      MobileActivator::PlanActivationState new_state,
      const std::string& error_description) OVERRIDE;

  // Callbacks for NetworkConfigurationHandler::GetProperties.
  void GetPropertiesAndCallStatusChanged(
      MobileActivator::PlanActivationState state,
      const std::string& error_description,
      const std::string& service_path,
      const base::DictionaryValue& properties);
  void GetPropertiesAndCallGetDeviceInfo(
      const std::string& service_path,
      const base::DictionaryValue& properties);
  void GetPropertiesFailure(
      const std::string& service_path,
      const std::string& callback_name,
      const std::string& error_name,
      scoped_ptr<base::DictionaryValue> error_data);

  // Handlers for JS WebUI messages.
  void HandleSetTransactionStatus(const base::ListValue* args);
  void HandleStartActivation(const base::ListValue* args);
  void HandlePaymentPortalLoad(const base::ListValue* args);
  void HandleGetDeviceInfo(const base::ListValue* args);

  // NetworkStateHandlerObserver implementation.
  virtual void NetworkConnectionStateChanged(
      const NetworkState* network) OVERRIDE;
  virtual void DefaultNetworkChanged(
      const NetworkState* default_network) OVERRIDE;

  // Updates |lte_portal_reachable_| for lte network |network| and notifies
  // webui of the new state if the reachability changed or |force_notification|
  // is set.
  void UpdatePortalReachability(const NetworkState* network,
                                bool force_notification);

  // Sends message to host registration page with system/user info data.
  void SendDeviceInfo();

  // Type of the mobilesetup webui deduced from received messages.
  Type type_;
  // Whether portal page for lte networks can be reached in current network
  // connection state. This value is reflected in portal webui for lte networks.
  // Initial value is true.
  bool lte_portal_reachable_;
  base::WeakPtrFactory<MobileSetupHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MobileSetupHandler);
};

////////////////////////////////////////////////////////////////////////////////
//
// MobileSetupUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

MobileSetupUIHTMLSource::MobileSetupUIHTMLSource()
    : weak_ptr_factory_(this) {
}

std::string MobileSetupUIHTMLSource::GetSource() const {
  return chrome::kChromeUIMobileSetupHost;
}

void MobileSetupUIHTMLSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  NetworkHandler::Get()->network_configuration_handler()->GetProperties(
      path,
      base::Bind(&MobileSetupUIHTMLSource::GetPropertiesAndStartDataRequest,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&MobileSetupUIHTMLSource::GetPropertiesFailure,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback, path));
}

void MobileSetupUIHTMLSource::GetPropertiesAndStartDataRequest(
    const content::URLDataSource::GotDataCallback& callback,
    const std::string& service_path,
    const base::DictionaryValue& properties) {
  const base::DictionaryValue* payment_dict;
  std::string name, usage_url, activation_state, payment_url;
  if (!properties.GetStringWithoutPathExpansion(
          shill::kNameProperty, &name) ||
      !properties.GetStringWithoutPathExpansion(
          shill::kUsageURLProperty, &usage_url) ||
      !properties.GetStringWithoutPathExpansion(
          shill::kActivationStateProperty, &activation_state) ||
      !properties.GetDictionaryWithoutPathExpansion(
          shill::kPaymentPortalProperty, &payment_dict) ||
      !payment_dict->GetStringWithoutPathExpansion(
          shill::kPaymentPortalURL, &payment_url)) {
    DataRequestFailed(service_path, callback);
    return;
  }

  if (payment_url.empty() && usage_url.empty() &&
      activation_state != shill::kActivationStateActivated) {
    DataRequestFailed(service_path, callback);
    return;
  }

  NET_LOG_EVENT("Starting mobile setup", service_path);
  base::DictionaryValue strings;

  strings.SetString("connecting_header",
                    l10n_util::GetStringFUTF16(IDS_MOBILE_CONNECTING_HEADER,
                                               base::UTF8ToUTF16(name)));
  strings.SetString("error_header",
                    l10n_util::GetStringUTF16(IDS_MOBILE_ERROR_HEADER));
  strings.SetString("activating_header",
                    l10n_util::GetStringUTF16(IDS_MOBILE_ACTIVATING_HEADER));
  strings.SetString("completed_header",
                    l10n_util::GetStringUTF16(IDS_MOBILE_COMPLETED_HEADER));
  strings.SetString("please_wait",
                    l10n_util::GetStringUTF16(IDS_MOBILE_PLEASE_WAIT));
  strings.SetString("completed_text",
                    l10n_util::GetStringUTF16(IDS_MOBILE_COMPLETED_TEXT));
  strings.SetString("portal_unreachable_header",
                    l10n_util::GetStringUTF16(IDS_MOBILE_NO_CONNECTION_HEADER));
  strings.SetString("invalid_device_info_header",
      l10n_util::GetStringUTF16(IDS_MOBILE_INVALID_DEVICE_INFO_HEADER));
  strings.SetString("title", l10n_util::GetStringUTF16(IDS_MOBILE_SETUP_TITLE));
  strings.SetString("close_button",
                    l10n_util::GetStringUTF16(IDS_CLOSE));
  strings.SetString("cancel_button",
                    l10n_util::GetStringUTF16(IDS_CANCEL));
  strings.SetString("ok_button",
                    l10n_util::GetStringUTF16(IDS_OK));
  webui::SetFontAndTextDirection(&strings);

  // The webui differs based on whether the network is activated or not. If the
  // network is activated, the webui goes straight to portal. Otherwise the
  // webui is used for activation flow.
  std::string full_html;
  if (activation_state == shill::kActivationStateActivated) {
    static const base::StringPiece html_for_activated(
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_MOBILE_SETUP_PORTAL_PAGE_HTML));
    full_html = webui::GetI18nTemplateHtml(html_for_activated, &strings);
  } else {
    static const base::StringPiece html_for_non_activated(
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_MOBILE_SETUP_PAGE_HTML));
    full_html = webui::GetI18nTemplateHtml(html_for_non_activated, &strings);
  }

  callback.Run(base::RefCountedString::TakeString(&full_html));
}

void MobileSetupUIHTMLSource::GetPropertiesFailure(
    const content::URLDataSource::GotDataCallback& callback,
    const std::string& service_path,
    const std::string& error_name,
    scoped_ptr<base::DictionaryValue> error_data) {
  DataRequestFailed(service_path, callback);
}

////////////////////////////////////////////////////////////////////////////////
//
// MobileSetupHandler
//
////////////////////////////////////////////////////////////////////////////////
MobileSetupHandler::MobileSetupHandler()
    : type_(TYPE_UNDETERMINED),
      lte_portal_reachable_(true),
      weak_ptr_factory_(this) {
}

MobileSetupHandler::~MobileSetupHandler() {
  if (type_ == TYPE_ACTIVATION) {
    MobileActivator::GetInstance()->RemoveObserver(this);
    MobileActivator::GetInstance()->TerminateActivation();
  } else if (type_ == TYPE_PORTAL_LTE) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                   FROM_HERE);
  }
}

void MobileSetupHandler::OnActivationStateChanged(
    const NetworkState* network,
    MobileActivator::PlanActivationState state,
    const std::string& error_description) {
  DCHECK_EQ(TYPE_ACTIVATION, type_);
  if (!web_ui())
    return;

  if (!network) {
    base::DictionaryValue device_dict;
    SetActivationStateAndError(state, error_description, &device_dict);
    web_ui()->CallJavascriptFunction(kJsDeviceStatusChangedCallback,
                                     device_dict);
    return;
  }

  NetworkHandler::Get()->network_configuration_handler()->GetProperties(
      network->path(),
      base::Bind(&MobileSetupHandler::GetPropertiesAndCallStatusChanged,
                 weak_ptr_factory_.GetWeakPtr(),
                 state,
                 error_description),
      base::Bind(&MobileSetupHandler::GetPropertiesFailure,
                 weak_ptr_factory_.GetWeakPtr(),
                 network->path(),
                 kJsDeviceStatusChangedCallback));
}

void MobileSetupHandler::GetPropertiesAndCallStatusChanged(
    MobileActivator::PlanActivationState state,
    const std::string& error_description,
    const std::string& service_path,
    const base::DictionaryValue& properties) {
  base::DictionaryValue device_dict;
  GetDeviceInfo(properties, &device_dict);
  SetActivationStateAndError(state, error_description, &device_dict);
  web_ui()->CallJavascriptFunction(kJsDeviceStatusChangedCallback, device_dict);
}

void MobileSetupHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(kJsApiStartActivation,
      base::Bind(&MobileSetupHandler::HandleStartActivation,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kJsApiSetTransactionStatus,
      base::Bind(&MobileSetupHandler::HandleSetTransactionStatus,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kJsApiPaymentPortalLoad,
      base::Bind(&MobileSetupHandler::HandlePaymentPortalLoad,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kJsGetDeviceInfo,
      base::Bind(&MobileSetupHandler::HandleGetDeviceInfo,
                 base::Unretained(this)));
}

void MobileSetupHandler::HandleStartActivation(const base::ListValue* args) {
  DCHECK_EQ(TYPE_UNDETERMINED, type_);

  if (!web_ui())
    return;

  std::string path = web_ui()->GetWebContents()->GetURL().path();
  if (!path.size())
    return;

  LOG(WARNING) << "Starting activation for service " << path;

  type_ = TYPE_ACTIVATION;
  MobileActivator::GetInstance()->AddObserver(this);
  MobileActivator::GetInstance()->InitiateActivation(path.substr(1));
}

void MobileSetupHandler::HandleSetTransactionStatus(
    const base::ListValue* args) {
  DCHECK_EQ(TYPE_ACTIVATION, type_);
  if (!web_ui())
    return;

  const size_t kSetTransactionStatusParamCount = 1;
  if (args->GetSize() != kSetTransactionStatusParamCount)
    return;
  // Get change callback function name.
  std::string status;
  if (!args->GetString(0, &status))
    return;

  MobileActivator::GetInstance()->OnSetTransactionStatus(
      LowerCaseEqualsASCII(status, kJsApiResultOK));
}

void MobileSetupHandler::HandlePaymentPortalLoad(const base::ListValue* args) {
  // Only activation flow webui is interested in these events.
  if (type_ != TYPE_ACTIVATION || !web_ui())
    return;

  const size_t kPaymentPortalLoadParamCount = 1;
  if (args->GetSize() != kPaymentPortalLoadParamCount)
    return;
  // Get change callback function name.
  std::string result;
  if (!args->GetString(0, &result))
    return;

  MobileActivator::GetInstance()->OnPortalLoaded(
      LowerCaseEqualsASCII(result, kJsApiResultOK));
}

void MobileSetupHandler::HandleGetDeviceInfo(const base::ListValue* args) {
  DCHECK_NE(TYPE_ACTIVATION, type_);
  if (!web_ui())
    return;

  std::string path = web_ui()->GetWebContents()->GetURL().path();
  if (path.empty())
    return;

  chromeos::NetworkStateHandler* nsh =
      NetworkHandler::Get()->network_state_handler();
  // TODO: Figure out why the path has an extra '/' in the front. (e.g. It is
  // '//service/5' instead of '/service/5'.
  const NetworkState* network = nsh->GetNetworkState(path.substr(1));
  if (!network) {
    web_ui()->GetWebContents()->Close();
    return;
  }

  // If this is the initial call, update the network status and start observing
  // network changes, but only for LTE networks. The other networks should
  // ignore network status.
  if (type_ == TYPE_UNDETERMINED) {
    if (network->network_technology() == shill::kNetworkTechnologyLte ||
        network->network_technology() == shill::kNetworkTechnologyLteAdvanced) {
      type_ = TYPE_PORTAL_LTE;
      nsh->AddObserver(this, FROM_HERE);
      // Update the network status and notify the webui. This is the initial
      // network state so the webui should be notified no matter what.
      UpdatePortalReachability(network,
                               true /* force notification */);
    } else {
      type_ = TYPE_PORTAL;
      // For non-LTE networks network state is ignored, so report the portal is
      // reachable, so it gets shown.
      web_ui()->CallJavascriptFunction(kJsConnectivityChangedCallback,
                                       base::FundamentalValue(true));
    }
  }

  NetworkHandler::Get()->network_configuration_handler()->GetProperties(
      network->path(),
      base::Bind(&MobileSetupHandler::GetPropertiesAndCallGetDeviceInfo,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&MobileSetupHandler::GetPropertiesFailure,
                 weak_ptr_factory_.GetWeakPtr(),
                 network->path(),
                 kJsGetDeviceInfoCallback));
}

void MobileSetupHandler::GetPropertiesAndCallGetDeviceInfo(
    const std::string& service_path,
    const base::DictionaryValue& properties) {
  base::DictionaryValue device_info;
  GetDeviceInfo(properties, &device_info);
  web_ui()->CallJavascriptFunction(kJsGetDeviceInfoCallback, device_info);
}

void MobileSetupHandler::GetPropertiesFailure(
    const std::string& service_path,
    const std::string& callback_name,
    const std::string& error_name,
    scoped_ptr<base::DictionaryValue> error_data) {
  NET_LOG_ERROR("MobileActivator GetProperties Failed: " + error_name,
                service_path);
  // Invoke |callback_name| with an empty dictionary.
  base::DictionaryValue device_dict;
  web_ui()->CallJavascriptFunction(callback_name, device_dict);
}

void MobileSetupHandler::DefaultNetworkChanged(
    const NetworkState* default_network) {
  if (!web_ui())
    return;

  std::string path = web_ui()->GetWebContents()->GetURL().path().substr(1);
  if (path.empty())
    return;

  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkState(path);
  if (!network) {
    LOG(ERROR) << "Service path lost";
    web_ui()->GetWebContents()->Close();
    return;
  }

  UpdatePortalReachability(network, false /* do not force notification */);
}

void MobileSetupHandler::NetworkConnectionStateChanged(
    const NetworkState* network) {
  if (!web_ui())
    return;

  std::string path = web_ui()->GetWebContents()->GetURL().path().substr(1);
  if (path.empty() || path != network->path())
    return;

  UpdatePortalReachability(network, false /* do not force notification */);
}

void MobileSetupHandler::UpdatePortalReachability(
    const NetworkState* network,
    bool force_notification) {
  DCHECK(web_ui());

  DCHECK_EQ(type_, TYPE_PORTAL_LTE);

  chromeos::NetworkStateHandler* nsh =
      NetworkHandler::Get()->network_state_handler();
  bool portal_reachable =
      (network->IsConnectedState() ||
       (nsh->DefaultNetwork() &&
        nsh->DefaultNetwork()->connection_state() == shill::kStateOnline));

  if (force_notification || portal_reachable != lte_portal_reachable_) {
    web_ui()->CallJavascriptFunction(kJsConnectivityChangedCallback,
                                     base::FundamentalValue(portal_reachable));
  }

  lte_portal_reachable_ = portal_reachable;
}

////////////////////////////////////////////////////////////////////////////////
//
// MobileSetupUI
//
////////////////////////////////////////////////////////////////////////////////

MobileSetupUI::MobileSetupUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new MobileSetupHandler());
  MobileSetupUIHTMLSource* html_source = new MobileSetupUIHTMLSource();

  // Set up the chrome://mobilesetup/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(profile, html_source);

  content::WebContentsObserver::Observe(web_ui->GetWebContents());
}

void MobileSetupUI::DidCommitProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& url,
    content::PageTransition transition_type) {
  if (render_frame_host->GetFrameName() != "paymentForm")
    return;

  web_ui()->CallJavascriptFunction(
        kJsPortalFrameLoadCompletedCallback);
}

void MobileSetupUI::DidFailProvisionalLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  if (render_frame_host->GetFrameName() != "paymentForm")
    return;

  base::FundamentalValue result_value(-error_code);
  web_ui()->CallJavascriptFunction(kJsPortalFrameLoadFailedCallback,
                                   result_value);
}
