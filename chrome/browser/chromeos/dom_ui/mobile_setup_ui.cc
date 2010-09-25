// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/mobile_setup_ui.h"

#include <string>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/weak_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/cros/system_library.h"

namespace {

// Host page JS API function names.
const char kJsApiGetDeviceInfo[] = "getDeviceInfo";
const char kJsApiSetTransactionStatus[] = "setTransactionStatus";

const wchar_t kJsDeviceStatusChangedHandler[] =
    L"mobile.MobileSetup.onDeviceStatusChanged";

}  // namespace

class MobileSetupUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  MobileSetupUIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  ~MobileSetupUIHTMLSource() {}

  DISALLOW_COPY_AND_ASSIGN(MobileSetupUIHTMLSource);
};

// The handler for Javascript messages related to the "register" view.
class MobileSetupHandler : public DOMMessageHandler,
                           public base::SupportsWeakPtr<MobileSetupHandler> {
 public:
  MobileSetupHandler();
  virtual ~MobileSetupHandler();

  // Init work after Attach.
  void Init();

  // DOMMessageHandler implementation.
  virtual DOMMessageHandler* Attach(DOMUI* dom_ui);
  virtual void RegisterMessages();

 private:
  // Handlers for JS DOMUI messages.
  void HandleGetDeviceInfo(const ListValue* args);
  void HandleSetTransactionStatus(const ListValue* args);

  // Sends message to host registration page with system/user info data.
  void SendDeviceInfo();

  // Converts device the current CelularNetork into JS object.
  bool GetDeviceInfo(DictionaryValue* value);

  DISALLOW_COPY_AND_ASSIGN(MobileSetupHandler);
};

////////////////////////////////////////////////////////////////////////////////
//
// MobileSetupUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

MobileSetupUIHTMLSource::MobileSetupUIHTMLSource()
    : DataSource(chrome::kChromeUIMobileSetupHost, MessageLoop::current()) {
}

void MobileSetupUIHTMLSource::StartDataRequest(const std::string& path,
                                                bool is_off_the_record,
                                                int request_id) {
  DictionaryValue strings;
  strings.SetString("title", l10n_util::GetStringUTF16(IDS_MOBILE_SETUP_TITLE));
  strings.SetString("header",
                    l10n_util::GetStringUTF16(IDS_MOBILE_SETUP_HEADER));
  SetFontAndTextDirection(&strings);

  static const base::StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_MOBILE_SETUP_PAGE_HTML));
  const std::string full_html = jstemplate_builder::GetTemplatesHtml(
      html, &strings, "t" /* template root node id */);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

////////////////////////////////////////////////////////////////////////////////
//
// MobileSetupHandler
//
////////////////////////////////////////////////////////////////////////////////
MobileSetupHandler::MobileSetupHandler() {
}

MobileSetupHandler::~MobileSetupHandler() {
}

DOMMessageHandler* MobileSetupHandler::Attach(DOMUI* dom_ui) {
  return DOMMessageHandler::Attach(dom_ui);
}

void MobileSetupHandler::Init() {
  // TODO(zelidag): Register for network change notification to make sure
  // that the status of the cellular network does not flip to something we
  // can't handle (disabled, suddenly activated by divine intervention...).
}

void MobileSetupHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback(kJsApiGetDeviceInfo,
      NewCallback(this, &MobileSetupHandler::HandleGetDeviceInfo));
  dom_ui_->RegisterMessageCallback(kJsApiSetTransactionStatus,
      NewCallback(this, &MobileSetupHandler::HandleSetTransactionStatus));
}

void MobileSetupHandler::HandleGetDeviceInfo(const ListValue* args) {
  const size_t kGetDeviceInfoParamCount = 1;
  if (args->GetSize() != kGetDeviceInfoParamCount)
    return;

  // Get change callback function name.
  string16 callback_func_name;
  if (!args->GetString(0, &callback_func_name))
    return;

  DictionaryValue value;
  if (GetDeviceInfo(&value))
    dom_ui_->CallJavascriptFunction(UTF16ToWide(callback_func_name), value);
}

void MobileSetupHandler::HandleSetTransactionStatus(const ListValue* args) {
  const size_t kSetTransactionStatusParamCount = 1;
  if (args->GetSize() != kSetTransactionStatusParamCount)
    return;

  // Get change callback function name.
  std::string status;
  if (!args->GetString(0, &status))
    return;

  chromeos::NetworkLibrary* network_lib =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  const chromeos::CellularNetworkVector& cell_networks =
      network_lib->cellular_networks();
  if (!cell_networks.size())
    return;
  const chromeos::CellularNetwork& network = *(cell_networks.begin());

  if (LowerCaseEqualsASCII(status, "OK") && network.StartActivation()) {
    DictionaryValue value;
    if (GetDeviceInfo(&value))
      dom_ui_->CallJavascriptFunction(kJsDeviceStatusChangedHandler, value);
  }
  // TODO(zelidrag): Close the setup tab automatically when payment fails?
  // Pending UX decision on this one.
}


bool MobileSetupHandler::GetDeviceInfo(DictionaryValue* value) {
  DCHECK(value);
  chromeos::NetworkLibrary* network_lib =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();

  const chromeos::CellularNetworkVector& cell_networks =
      network_lib->cellular_networks();
  if (!cell_networks.size())
    return false;

  const chromeos::CellularNetwork& network = *(cell_networks.begin());

  value->SetString("carrier", UTF8ToUTF16(network.name()));
  value->SetString("payment_url", UTF8ToUTF16(network.payment_url()));
  value->SetString("activation_state", UTF8ToUTF16(network.activation_state()));
  value->SetString("MEID", UTF8ToUTF16(network.meid()));
  value->SetString("IMEI", UTF8ToUTF16(network.imei()));
  value->SetString("IMSI", UTF8ToUTF16(network.imsi()));
  value->SetString("ESN", UTF8ToUTF16(network.esn()));
  value->SetString("MDN", UTF8ToUTF16(network.mdn()));
  return true;
}

////////////////////////////////////////////////////////////////////////////////
//
// MobileSetupUI
//
////////////////////////////////////////////////////////////////////////////////

MobileSetupUI::MobileSetupUI(TabContents* contents) : DOMUI(contents){
  MobileSetupHandler* handler = new MobileSetupHandler();
  AddMessageHandler((handler)->Attach(this));
  handler->Init();
  MobileSetupUIHTMLSource* html_source = new MobileSetupUIHTMLSource();

  // Set up the chrome://mobilesetup/ source.
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(html_source)));
}
