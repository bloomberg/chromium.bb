// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/mobile_setup_ui.h"

#include <map>
#include <string>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/weak_ptr.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/cros/system_library.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

namespace {

// Host page JS API function names.
const char kJsApiCloseTab[] = "closeTab";
const char kJsApiSetTransactionStatus[] = "setTransactionStatus";

const wchar_t kJsDeviceStatusChangedHandler[] =
    L"handler.MobileSetup.deviceStateChanged";

// Collular device states that are reported to DOM UI layer.
const char kStateUnknown[] = "unknown";
const char kStateConnecting[] = "connecting";
const char kStateError[] = "error";
const char kStateNeedsPayment[] = "payment";
const char kStateActivating[] = "activating";
const char kStateDisconnected[] = "disconnected";
const char kStateConnected[] = "connected";
const char kFailedPayment[] = "failed_payment";

// Error codes matching codes defined in the cellular config file.
const char kErrorDefault[] = "default";
const char kErrorBadConnectionPartial[] = "bad_connection_partial";
const char kErrorBadConnectionActivated[] = "bad_connection_activated";
const char kErrorRoamingOnConnection[] = "roaming_connection";
const char kErrorNoEVDO[] = "no_evdo";
const char kErrorRoamingActivation[] = "roaming_activation";
const char kErrorRoamingPartiallyActivated[] = "roaming_partially_activated";
const char kErrorNoService[] = "no_service";
const char kFailedPaymentError[] = "failed_payment";

// Cellular configuration file path.
const char kCellularConfigPath[] =
    "/usr/share/chromeos-assets/mobile/mobile_config.json";

// Cellular config file field names.
const char kVersionField[] = "version";
const char kErrorsField[] = "errors";

}  // namespace

class CellularConfigDocument {
 public:
  CellularConfigDocument() {}

  // Return error message for a given code.
  std::string GetErrorMessage(const std::string& code);
  const std::string& version() { return version_; }

  bool LoadFromFile(const FilePath& config_path);

 private:
  std::string version_;
  std::map<std::string, std::string> error_map_;

  DISALLOW_COPY_AND_ASSIGN(CellularConfigDocument);
};

static std::map<std::string, std::string> error_messages_;

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
                           public chromeos::NetworkLibrary::Observer,
                           public base::SupportsWeakPtr<MobileSetupHandler> {
 public:
  MobileSetupHandler();
  virtual ~MobileSetupHandler();

  // Init work after Attach.
  void Init(TabContents* contents);

  // DOMMessageHandler implementation.
  virtual DOMMessageHandler* Attach(DOMUI* dom_ui);
  virtual void RegisterMessages();

  // NetworkLibrary::Observer implementation.
  virtual void NetworkChanged(chromeos::NetworkLibrary* obj);

 private:
  // Handlers for JS DOMUI messages.
  void HandleCloseTab(const ListValue* args);
  void HandleSetTransactionStatus(const ListValue* args);

  // Sends message to host registration page with system/user info data.
  void SendDeviceInfo();

  // Converts the currently active CellularNetwork device into a JS object.
  static bool GetDeviceInfo(DictionaryValue* value);
  static bool ShouldReportDeviceState(std::string* state, std::string* error);

  // Performs activation state cellular device evaluation.
  // Returns false if device activation failed. In this case |error|
  // will contain error message to be reported to DOM UI.
  static bool CheckForActivationError(chromeos::CellularNetwork network,
                                      std::string* error);

  // Return error message for a given code.
  static std::string GetErrorMessage(const std::string& code);
  static void LoadCellularConfig();

  static scoped_ptr<CellularConfigDocument> cellular_config_;

  TabContents* tab_contents_;
  DISALLOW_COPY_AND_ASSIGN(MobileSetupHandler);
};

scoped_ptr<CellularConfigDocument> MobileSetupHandler::cellular_config_;

////////////////////////////////////////////////////////////////////////////////
//
// CellularConfigDocument
//
////////////////////////////////////////////////////////////////////////////////

std::string CellularConfigDocument::GetErrorMessage(const std::string& code) {
  std::map<std::string, std::string>::iterator iter = error_map_.find(code);
  if (iter == error_map_.end())
    return code;
  return iter->second;
}

bool CellularConfigDocument::LoadFromFile(const FilePath& config_path) {
  error_map_.clear();

  std::string config;
  if (!file_util::ReadFileToString(config_path, &config))
    return false;

  scoped_ptr<Value> root(base::JSONReader::Read(config, true));
  DCHECK(root.get() != NULL);
  if (!root.get() || root->GetType() != Value::TYPE_DICTIONARY) {
    VLOG(1) << "Bad cellular config file";
    return false;
  }

  DictionaryValue* root_dict = static_cast<DictionaryValue*>(root.get());
  if (!root_dict->GetString(kVersionField, &version_)) {
    VLOG(1) << "Cellular config file missing version";
    return false;
  }
  DictionaryValue* errors = NULL;
  if (!root_dict->GetDictionary(kErrorsField, &errors))
    return false;
  for (DictionaryValue::key_iterator keys = errors->begin_keys();
       keys != errors->end_keys();
       ++keys) {
    std::string value;
    if (!errors->GetString(*keys, &value)) {
      VLOG(1) << "Bad cellular config error value";
      error_map_.clear();
      return false;
    }

    error_map_.insert(std::pair<std::string, std::string>(*keys, value));
  }
  return true;
}

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
  strings.SetString("connecting_header",
                    l10n_util::GetStringUTF16(IDS_MOBILE_CONNECTING_HEADER));
  strings.SetString("error_header",
                    l10n_util::GetStringUTF16(IDS_MOBILE_ERROR_HEADER));
  strings.SetString("activating_header",
                    l10n_util::GetStringUTF16(IDS_MOBILE_ACTIVATING_HEADER));
  strings.SetString("completed_header",
                    l10n_util::GetStringUTF16(IDS_MOBILE_COMPLETED_HEADER));
  strings.SetString("completed_text",
                    l10n_util::GetStringUTF16(IDS_MOBILE_COMPLETED_TEXT));
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
MobileSetupHandler::MobileSetupHandler() : tab_contents_(NULL) {
}

MobileSetupHandler::~MobileSetupHandler() {
  chromeos::CrosLibrary::Get()->GetNetworkLibrary()->RemoveObserver(this);
}

DOMMessageHandler* MobileSetupHandler::Attach(DOMUI* dom_ui) {
  return DOMMessageHandler::Attach(dom_ui);
}

void MobileSetupHandler::Init(TabContents* contents) {
  tab_contents_ = contents;
  chromeos::CrosLibrary::Get()->GetNetworkLibrary()->AddObserver(this);

  // TODO(zelidrag): We might want to move this to another thread.
  LoadCellularConfig();
}

void MobileSetupHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback(kJsApiCloseTab,
      NewCallback(this, &MobileSetupHandler::HandleCloseTab));
  dom_ui_->RegisterMessageCallback(kJsApiSetTransactionStatus,
      NewCallback(this, &MobileSetupHandler::HandleSetTransactionStatus));
}

void MobileSetupHandler::NetworkChanged(chromeos::NetworkLibrary* cros) {
  if (!dom_ui_)
    return;
  DictionaryValue device;
  GetDeviceInfo(&device);
  std::string state, error;
  if (!ShouldReportDeviceState(&state, &error))
    return;

  device.SetString("state", state);
  if (error.length())
    device.SetString("error", error);
  dom_ui_->CallJavascriptFunction(
      kJsDeviceStatusChangedHandler, device);
}

void MobileSetupHandler::HandleCloseTab(const ListValue* args) {
  Browser* browser = BrowserList::GetLastActive();
  if (browser)
    browser->CloseTabContents(tab_contents_);
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

  // We assume only one cellular network will come from flimflam for now.
  const chromeos::CellularNetwork& network = *(cell_networks.begin());

  if (LowerCaseEqualsASCII(status, "OK")) {
    network.StartActivation();
  } else {
    DictionaryValue value;
    value.SetString("state", kFailedPaymentError);
    dom_ui_->CallJavascriptFunction(kJsDeviceStatusChangedHandler, value);
  }
}

bool MobileSetupHandler::ShouldReportDeviceState(std::string* state,
                                                 std::string* error) {
  DCHECK(state);
  DCHECK(error);
  chromeos::NetworkLibrary* network_lib =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();

  const chromeos::CellularNetworkVector& cell_networks =
      network_lib->cellular_networks();
  // No cellular network present? Treat as network is disconnected.
  // This could be transient state that is the result of activation process.
  if (!cell_networks.size()) {
    *state = kStateDisconnected;
    return true;
  }
  const chromeos::CellularNetwork& network = cell_networks.at(0);

  // First, check if device activation / plan payment failed.
  // It's slightly more complex than just single state check
  // that we are doing for other states below.
  if (!CheckForActivationError(network, error)) {
    *state = kStateError;
    return true;
  }

  switch (network.activation_state()) {
    case chromeos::ACTIVATION_STATE_UNKNOWN:
    case chromeos::ACTIVATION_STATE_NOT_ACTIVATED:
      // If this page is shown, I assume that we have already kicked off the
      // process of starting the activation even though the device status
      // reporting might not have caught up with us yet.
      *state = kStateConnecting;
      return true;
    case chromeos::ACTIVATION_STATE_PARTIALLY_ACTIVATED:
    case chromeos::ACTIVATION_STATE_ACTIVATING:
      *state = kStateActivating;
      return true;
    case chromeos::ACTIVATION_STATE_ACTIVATED:
      *state = kStateConnected;
      return true;
  }

  // We don't report states that we don't understand to DOM UI.
  *state = kStateUnknown;
  return false;
}


bool MobileSetupHandler::CheckForActivationError(
    chromeos::CellularNetwork network, std::string* error) {
  bool got_error = false;
  const char* error_code = kErrorDefault;

  // This is the magic of error selecting based
  if (network.connection_state() == chromeos::STATE_FAILURE &&
      network.error() == chromeos::ERROR_AAA_FAILED ) {
    if (network.activation_state() ==
            chromeos::ACTIVATION_STATE_PARTIALLY_ACTIVATED) {
      error_code = kErrorBadConnectionPartial;
    } else if (network.activation_state() ==
            chromeos::ACTIVATION_STATE_ACTIVATED) {
      if (network.roaming_state() == chromeos::ROAMING_STATE_HOME) {
        error_code = kErrorBadConnectionActivated;
      } else if (network.roaming_state() == chromeos::ROAMING_STATE_ROAMING) {
        error_code = kErrorRoamingOnConnection;
      }
    }
    got_error = true;
  } else if (network.connection_state() ==
                 chromeos::STATE_ACTIVATION_FAILURE) {
    if (network.error() == chromeos::ERROR_NEED_EVDO) {
      if (network.activation_state() ==
              chromeos::ACTIVATION_STATE_PARTIALLY_ACTIVATED)
        error_code = kErrorNoEVDO;
    } else if (network.error() == chromeos::ERROR_NEED_HOME_NETWORK) {
      if (network.activation_state() ==
              chromeos::ACTIVATION_STATE_NOT_ACTIVATED) {
        error_code = kErrorRoamingActivation;
      } else if (network.activation_state() ==
                    chromeos::ACTIVATION_STATE_PARTIALLY_ACTIVATED) {
        error_code = kErrorRoamingPartiallyActivated;
      }
    }
    got_error = true;
  }

  if (got_error)
    *error = GetErrorMessage(error_code);

  return !got_error;
}

bool MobileSetupHandler::GetDeviceInfo(DictionaryValue* value) {
  DCHECK(value);
  chromeos::NetworkLibrary* network_lib =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();

  const chromeos::CellularNetworkVector& cell_networks =
      network_lib->cellular_networks();
  if (!cell_networks.size()) {
    return false;
  }

  const chromeos::CellularNetwork& network = *(cell_networks.begin());
  value->SetString("carrier", UTF8ToUTF16(network.name()));
  value->SetString("payment_url", UTF8ToUTF16(network.payment_url()));
  value->SetString("MEID", UTF8ToUTF16(network.meid()));
  value->SetString("IMEI", UTF8ToUTF16(network.imei()));
  value->SetString("IMSI", UTF8ToUTF16(network.imsi()));
  value->SetString("ESN", UTF8ToUTF16(network.esn()));
  value->SetString("MDN", UTF8ToUTF16(network.mdn()));
  return true;
}

std::string MobileSetupHandler::GetErrorMessage(const std::string& code) {
  if (!cellular_config_.get())
    return "";
  return cellular_config_->GetErrorMessage(code);
}

void MobileSetupHandler::LoadCellularConfig() {
  static bool config_loaded = false;
  if (config_loaded)
    return;
  config_loaded = true;
  // Load partner customization startup manifest if it is available.
  FilePath config_path(kCellularConfigPath);
  if (file_util::PathExists(config_path)) {
    scoped_ptr<CellularConfigDocument> config(new CellularConfigDocument());
    bool config_loaded = config->LoadFromFile(config_path);
    if (config_loaded) {
      VLOG(1) << "Cellular config file loaded: " << kCellularConfigPath;
      // lock
      cellular_config_.reset(config.release());
    } else {
      LOG(ERROR) << "Error loading cellular config file: " <<
          kCellularConfigPath;
    }
  }
}


////////////////////////////////////////////////////////////////////////////////
//
// MobileSetupUI
//
////////////////////////////////////////////////////////////////////////////////

MobileSetupUI::MobileSetupUI(TabContents* contents) : DOMUI(contents){
  MobileSetupHandler* handler = new MobileSetupHandler();
  AddMessageHandler((handler)->Attach(this));
  handler->Init(contents);
  MobileSetupUIHTMLSource* html_source = new MobileSetupUIHTMLSource();

  // Set up the chrome://mobilesetup/ source.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(html_source)));
}
