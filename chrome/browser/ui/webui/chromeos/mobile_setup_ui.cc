// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/mobile_setup_ui.h"

#include <algorithm>
#include <map>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/mobile/mobile_activator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host_observer.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/webui/jstemplate_builder.h"
#include "ui/webui/web_ui_util.h"

using chromeos::CellularNetwork;
using chromeos::CrosLibrary;
using chromeos::MobileActivator;
using content::BrowserThread;
using content::RenderViewHost;
using content::WebContents;
using content::WebUIMessageHandler;

namespace {

// Host page JS API function names.
const char kJsApiStartActivation[] = "startActivation";
const char kJsApiSetTransactionStatus[] = "setTransactionStatus";
const char kJsApiPaymentPortalLoad[] = "paymentPortalLoad";
const char kJsApiResultOK[] = "ok";

const char kJsDeviceStatusChangedCallback[] =
    "mobile.MobileSetup.deviceStateChanged";
const char kJsPortalFrameLoadFailedCallback[] =
    "mobile.MobileSetup.portalFrameLoadError";
const char kJsPortalFrameLoadCompletedCallback[] =
    "mobile.MobileSetup.portalFrameLoadCompleted";

}  // namespace

// Observes IPC messages from the rederer and notifies JS if frame loading error
// appears.
class PortalFrameLoadObserver : public content::RenderViewHostObserver {
 public:
  PortalFrameLoadObserver(const base::WeakPtr<MobileSetupUI>& parent,
                          RenderViewHost* host)
      : content::RenderViewHostObserver(host), parent_(parent) {
    Send(new ChromeViewMsg_StartFrameSniffer(routing_id(),
                                             UTF8ToUTF16("paymentForm")));
  }

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(PortalFrameLoadObserver, message)
      IPC_MESSAGE_HANDLER(ChromeViewHostMsg_FrameLoadingError, OnFrameLoadError)
      IPC_MESSAGE_HANDLER(ChromeViewHostMsg_FrameLoadingCompleted,
                          OnFrameLoadCompleted)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

 private:
  void OnFrameLoadError(int error) {
    if (!parent_.get())
      return;

    base::FundamentalValue result_value(error);
    parent_->web_ui()->CallJavascriptFunction(kJsPortalFrameLoadFailedCallback,
                                              result_value);
  }

  void OnFrameLoadCompleted() {
    if (!parent_.get())
      return;

    parent_->web_ui()->CallJavascriptFunction(
        kJsPortalFrameLoadCompletedCallback);
  }

  base::WeakPtr<MobileSetupUI> parent_;
  DISALLOW_COPY_AND_ASSIGN(PortalFrameLoadObserver);
};

class MobileSetupUIHTMLSource : public content::URLDataSource {
 public:
  MobileSetupUIHTMLSource();

  // content::URLDataSource implementation.
  virtual std::string GetSource() OVERRIDE;
  virtual void StartDataRequest(
      const std::string& path,
      bool is_incognito,
      const content::URLDataSource::GotDataCallback& callback) OVERRIDE;
  virtual std::string GetMimeType(const std::string&) const OVERRIDE {
    return "text/html";
  }
  virtual bool ShouldAddContentSecurityPolicy() const OVERRIDE {
    return false;
  }

 private:
  virtual ~MobileSetupUIHTMLSource() {}

  DISALLOW_COPY_AND_ASSIGN(MobileSetupUIHTMLSource);
};

// The handler for Javascript messages related to the "register" view.
class MobileSetupHandler
  : public WebUIMessageHandler,
    public MobileActivator::Observer,
    public base::SupportsWeakPtr<MobileSetupHandler> {
 public:
  MobileSetupHandler();
  virtual ~MobileSetupHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Changes internal state.
  void OnActivationStateChanged(CellularNetwork* network,
                                MobileActivator::PlanActivationState new_state,
                                const std::string& error_description);

  // Handlers for JS WebUI messages.
  void HandleSetTransactionStatus(const ListValue* args);
  void HandleStartActivation(const ListValue* args);
  void HandlePaymentPortalLoad(const ListValue* args);

  // Sends message to host registration page with system/user info data.
  void SendDeviceInfo();

  // Converts the currently active CellularNetwork device into a JS object.
  static void GetDeviceInfo(CellularNetwork* network,
                            DictionaryValue* value);

  DISALLOW_COPY_AND_ASSIGN(MobileSetupHandler);
};

////////////////////////////////////////////////////////////////////////////////
//
// MobileSetupUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

MobileSetupUIHTMLSource::MobileSetupUIHTMLSource() {
}

std::string MobileSetupUIHTMLSource::GetSource() {
  return chrome::kChromeUIMobileSetupHost;
}

void MobileSetupUIHTMLSource::StartDataRequest(
    const std::string& path,
    bool is_incognito,
    const content::URLDataSource::GotDataCallback& callback) {
  CellularNetwork* network = !path.size() ? NULL :
      CrosLibrary::Get()->
          GetNetworkLibrary()->FindCellularNetworkByPath(path);
  if (!network || !network->SupportsActivation()) {
    LOG(WARNING) << "Can't find device to activate for service path " << path;
    scoped_refptr<base::RefCountedBytes> html_bytes(new base::RefCountedBytes);
    callback.Run(html_bytes);
    return;
  }

  LOG(WARNING) << "Activating mobile service " << path;
  DictionaryValue strings;
  strings.SetString("title", l10n_util::GetStringUTF16(IDS_MOBILE_SETUP_TITLE));
  strings.SetString("connecting_header",
                    l10n_util::GetStringFUTF16(IDS_MOBILE_CONNECTING_HEADER,
                        network ? UTF8ToUTF16(network->name()) : string16()));
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
  strings.SetString("close_button",
                    l10n_util::GetStringUTF16(IDS_CLOSE));
  strings.SetString("cancel_button",
                    l10n_util::GetStringUTF16(IDS_CANCEL));
  strings.SetString("ok_button",
                    l10n_util::GetStringUTF16(IDS_OK));
  webui::SetFontAndTextDirection(&strings);

  static const base::StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_MOBILE_SETUP_PAGE_HTML));

  std::string full_html = webui::GetI18nTemplateHtml(html, &strings);

  callback.Run(base::RefCountedString::TakeString(&full_html));
}

////////////////////////////////////////////////////////////////////////////////
//
// MobileSetupHandler
//
////////////////////////////////////////////////////////////////////////////////
MobileSetupHandler::MobileSetupHandler() {
  MobileActivator::GetInstance()->AddObserver(this);
}

MobileSetupHandler::~MobileSetupHandler() {
  MobileActivator::GetInstance()->RemoveObserver(this);
  MobileActivator::GetInstance()->TerminateActivation();
}

void MobileSetupHandler::OnActivationStateChanged(
    CellularNetwork* network,
    MobileActivator::PlanActivationState state,
    const std::string& error_description) {
  if (!web_ui())
    return;

  DictionaryValue device_dict;
  if (network)
    GetDeviceInfo(network, &device_dict);
  device_dict.SetInteger("state", state);
  if (error_description.length())
    device_dict.SetString("error", error_description);
  web_ui()->CallJavascriptFunction(
      kJsDeviceStatusChangedCallback, device_dict);
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
}

void MobileSetupHandler::HandleStartActivation(const ListValue* args) {
  if (!web_ui())
    return;

  std::string path = web_ui()->GetWebContents()->GetURL().path();
  if (!path.size())
    return;

  LOG(WARNING) << "Starting activation for service " << path;

  MobileActivator::GetInstance()->InitiateActivation(path.substr(1));
}

void MobileSetupHandler::HandleSetTransactionStatus(const ListValue* args) {
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

void MobileSetupHandler::HandlePaymentPortalLoad(const ListValue* args) {
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

void MobileSetupHandler::GetDeviceInfo(CellularNetwork* network,
                                       DictionaryValue* value) {
  DCHECK(network);
  chromeos::NetworkLibrary* cros =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  if (!cros)
    return;
  value->SetString("carrier", network->name());
  value->SetString("payment_url", network->payment_url());
  if (network->using_post() && network->post_data().length())
    value->SetString("post_data", network->post_data());

  const chromeos::NetworkDevice* device =
      cros->FindNetworkDeviceByPath(network->device_path());
  if (device) {
    value->SetString("MEID", device->meid());
    value->SetString("IMEI", device->imei());
    value->SetString("MDN", device->mdn());
  }
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
  ChromeURLDataManager::AddDataSource(profile, html_source);
}

void MobileSetupUI::RenderViewCreated(RenderViewHost* host) {
  // Destroyed by the corresponding RenderViewHost
  new PortalFrameLoadObserver(AsWeakPtr(), host);
}
