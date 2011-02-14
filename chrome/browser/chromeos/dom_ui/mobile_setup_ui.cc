// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/mobile_setup_ui.h"

#include <algorithm>
#include <map>
#include <string>

#include "base/callback.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/timer.h"
#include "base/values.h"
#include "base/weak_ptr.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/cros/system_library.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Host page JS API function names.
const char kJsApiStartActivation[] = "startActivation";
const char kJsApiSetTransactionStatus[] = "setTransactionStatus";

const wchar_t kJsDeviceStatusChangedHandler[] =
    L"mobile.MobileSetup.deviceStateChanged";

// Error codes matching codes defined in the cellular config file.
const char kErrorDefault[] = "default";
const char kErrorBadConnectionPartial[] = "bad_connection_partial";
const char kErrorBadConnectionActivated[] = "bad_connection_activated";
const char kErrorRoamingOnConnection[] = "roaming_connection";
const char kErrorNoEVDO[] = "no_evdo";
const char kErrorRoamingActivation[] = "roaming_activation";
const char kErrorRoamingPartiallyActivated[] = "roaming_partially_activated";
const char kErrorNoService[] = "no_service";
const char kErrorDisabled[] = "disabled";
const char kErrorNoDevice[] = "no_device";
const char kFailedPaymentError[] = "failed_payment";
const char kFailedConnectivity[] = "connectivity";
const char kErrorAlreadyRunning[] = "already_running";

// Cellular configuration file path.
const char kCellularConfigPath[] =
    "/usr/share/chromeos-assets/mobile/mobile_config.json";

// Cellular config file field names.
const char kVersionField[] = "version";
const char kErrorsField[] = "errors";

// Number of times we will retry to restart the activation process in case
// there is no connectivity in the restricted pool.
const int kMaxActivationAttempt = 3;
// Number of times we will retry to reconnect if connection fails.
const int kMaxConnectionRetry = 10;
// Number of times we will retry to reconnect if connection fails.
const int kMaxConnectionRetryOTASP = 30;
// Number of times we will retry to reconnect after payment is processed.
const int kMaxReconnectAttemptOTASP = 30;
// Reconnect retry delay (after payment is processed).
const int kPostPaymentReconnectDelayMS = 30000;   // 30s.
// Connection timeout in seconds.
const int kConnectionTimeoutSeconds = 45;
// Reconnect delay.
const int kReconnectDelayMS = 3000;
// Reconnect timer delay.
const int kReconnectTimerDelayMS = 5000;
// Reconnect delay after previous failure.
const int kFailedReconnectDelayMS = 10000;
// Retry delay after failed OTASP attempt.
const int kOTASPRetryDelay = 20000;

chromeos::CellularNetwork* GetCellularNetwork() {
  chromeos::NetworkLibrary* lib = chromeos::CrosLibrary::Get()->
      GetNetworkLibrary();
  if (lib->cellular_networks().begin() != lib->cellular_networks().end()) {
    return *(lib->cellular_networks().begin());
  }
  return NULL;
}

chromeos::CellularNetwork* GetCellularNetwork(const std::string& service_path) {
  return chromeos::CrosLibrary::Get()->
      GetNetworkLibrary()->FindCellularNetworkByPath(service_path);
}

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
  explicit MobileSetupUIHTMLSource(const std::string& service_path);

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  virtual ~MobileSetupUIHTMLSource() {}

  std::string service_path_;
  DISALLOW_COPY_AND_ASSIGN(MobileSetupUIHTMLSource);
};

// The handler for Javascript messages related to the "register" view.
class MobileSetupHandler
  : public WebUIMessageHandler,
    public chromeos::NetworkLibrary::NetworkManagerObserver,
    public chromeos::NetworkLibrary::NetworkObserver,
    public base::SupportsWeakPtr<MobileSetupHandler> {
 public:
  explicit MobileSetupHandler(const std::string& service_path);
  virtual ~MobileSetupHandler();

  // Init work after Attach.
  void Init(TabContents* contents);

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui);
  virtual void RegisterMessages();

  // NetworkLibrary::NetworkManagerObserver implementation.
  virtual void OnNetworkManagerChanged(chromeos::NetworkLibrary* obj);
  // NetworkLibrary::NetworkObserver implementation.
  virtual void OnNetworkChanged(chromeos::NetworkLibrary* obj,
                                const chromeos::Network* network);

 private:
  typedef enum PlanActivationState {
    PLAN_ACTIVATION_PAGE_LOADING            = -1,
    PLAN_ACTIVATION_START                   = 0,
    PLAN_ACTIVATION_TRYING_OTASP            = 1,
    PLAN_ACTIVATION_RECONNECTING_OTASP_TRY  = 2,
    PLAN_ACTIVATION_INITIATING_ACTIVATION   = 3,
    PLAN_ACTIVATION_RECONNECTING            = 4,
    PLAN_ACTIVATION_SHOWING_PAYMENT         = 5,
    PLAN_ACTIVATION_DELAY_OTASP             = 6,
    PLAN_ACTIVATION_START_OTASP             = 7,
    PLAN_ACTIVATION_OTASP                   = 8,
    PLAN_ACTIVATION_RECONNECTING_OTASP      = 9,
    PLAN_ACTIVATION_DONE                    = 10,
    PLAN_ACTIVATION_ERROR                   = 0xFF,
  } PlanActivationState;

  class TaskProxy : public base::RefCountedThreadSafe<TaskProxy> {
   public:
    TaskProxy(const base::WeakPtr<MobileSetupHandler>& handler, int delay)
        : handler_(handler), delay_(delay) {
    }
    TaskProxy(const base::WeakPtr<MobileSetupHandler>& handler,
              const std::string& status)
        : handler_(handler), status_(status) {
    }
    void HandleStartActivation() {
      if (handler_)
        handler_->StartActivation();
    }
    void HandleSetTransactionStatus() {
      if (handler_)
        handler_->SetTransactionStatus(status_);
    }
    void ContinueConnecting() {
      if (handler_)
        handler_->ContinueConnecting(delay_);
    }
    void RetryOTASP() {
      if (handler_)
        handler_->RetryOTASP();
    }
   private:
    base::WeakPtr<MobileSetupHandler> handler_;
    std::string status_;
    int delay_;
    DISALLOW_COPY_AND_ASSIGN(TaskProxy);
  };

  // Handlers for JS WebUI messages.
  void HandleSetTransactionStatus(const ListValue* args);
  void HandleStartActivation(const ListValue* args);
  void SetTransactionStatus(const std::string& status);
  // Schedules activation process via task proxy.
  void InitiateActivation();
  // Starts activation.
  void StartActivation();
  // Retried OTASP.
  void RetryOTASP();
  // Continues activation process. This method is called after we disconnect
  // due to detected connectivity issue to kick off reconnection.
  void ContinueConnecting(int delay);

  // Sends message to host registration page with system/user info data.
  void SendDeviceInfo();

  // Callback for when |reconnect_timer_| fires.
  void ReconnectTimerFired();
  // Starts OTASP process.
  void StartOTASP();
  // Checks if we need to reconnect due to failed connection attempt.
  bool NeedsReconnecting(chromeos::CellularNetwork* network,
                         PlanActivationState* new_state,
                         std::string* error_description);
  // Disconnect from network.
  void DisconnectFromNetwork(chromeos::CellularNetwork* network);
  // Connects to cellular network, resets connection timer.
  bool ConnectToNetwork(chromeos::CellularNetwork* network, int delay);
  // Forces disconnect / reconnect when we detect portal connectivity issues.
  void ForceReconnect(chromeos::CellularNetwork* network, int delay);
  // Reports connection timeout.
  bool ConnectionTimeout();
  // Verify the state of cellular network and modify internal state.
  void EvaluateCellularNetwork(chromeos::CellularNetwork* network);
  // Check the current cellular network for error conditions.
  bool GotActivationError(const chromeos::CellularNetwork* network,
                          std::string* error);
  // Sends status updates to WebUI page.
  void UpdatePage(chromeos::CellularNetwork* network,
                  const std::string& error_description);
  // Changes internal state.
  void ChangeState(chromeos::CellularNetwork* network,
                   PlanActivationState new_state,
                   const std::string& error_description);
  // Prepares network devices for cellular activation process.
  void SetupActivationProcess(chromeos::CellularNetwork* network);
  // Disables ethernet and wifi newtorks since they interefere with
  // detection of restricted pool on cellular side.
  void DisableOtherNetworks();
  // Resets network devices after cellular activation process.
  // |network| should be NULL if the activation process failed.
  void CompleteActivation(chromeos::CellularNetwork* network);
  // Control routines for handling other types of connections during
  // cellular activation.
  void ReEnableOtherConnections();

  // Converts the currently active CellularNetwork device into a JS object.
  static void GetDeviceInfo(const chromeos::CellularNetwork* network,
                            DictionaryValue* value);
  static bool ShouldReportDeviceState(std::string* state, std::string* error);

  // Performs activation state cellular device evaluation.
  // Returns false if device activation failed. In this case |error|
  // will contain error message to be reported to Web UI.
  static bool EvaluateCellularDeviceState(bool* report_status,
                                          std::string* state,
                                          std::string* error);

  // Return error message for a given code.
  static std::string GetErrorMessage(const std::string& code);
  static void LoadCellularConfig();

  // Returns next reconnection state based on the current activation phase.
  static PlanActivationState GetNextReconnectState(PlanActivationState state);
  static const char* GetStateDescription(PlanActivationState state);

  static scoped_ptr<CellularConfigDocument> cellular_config_;

  TabContents* tab_contents_;
  // Internal handler state.
  PlanActivationState state_;
  std::string service_path_;
  // Flags that control if wifi and ethernet connection needs to be restored
  // after the activation of cellular network.
  bool reenable_wifi_;
  bool reenable_ethernet_;
  bool reenable_cert_check_;
  bool evaluating_;
  // True if we think that another tab is already running activation.
  bool already_running_;
  // Connection retry counter.
  int connection_retry_count_;
  // Post payment reconnect wait counters.
  int reconnect_wait_count_;
  // Activation retry attempt count;
  int activation_attempt_;
  // Connection start time.
  base::Time connection_start_time_;
  // Timer that monitors reconnection timeouts.
  base::RepeatingTimer<MobileSetupHandler> reconnect_timer_;

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
  {
    // Reading config file causes us to do blocking IO on UI thread.
    // Temporarily allow it until we fix http://crosbug.com/11535
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    if (!file_util::ReadFileToString(config_path, &config))
      return false;
  }

  scoped_ptr<Value> root(base::JSONReader::Read(config, true));
  DCHECK(root.get() != NULL);
  if (!root.get() || root->GetType() != Value::TYPE_DICTIONARY) {
    LOG(WARNING) << "Bad cellular config file";
    return false;
  }

  DictionaryValue* root_dict = static_cast<DictionaryValue*>(root.get());
  if (!root_dict->GetString(kVersionField, &version_)) {
    LOG(WARNING) << "Cellular config file missing version";
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
      LOG(WARNING) << "Bad cellular config error value";
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

MobileSetupUIHTMLSource::MobileSetupUIHTMLSource(
    const std::string& service_path)
    : DataSource(chrome::kChromeUIMobileSetupHost, MessageLoop::current()),
      service_path_(service_path) {
}

void MobileSetupUIHTMLSource::StartDataRequest(const std::string& path,
                                                bool is_off_the_record,
                                                int request_id) {
  const chromeos::CellularNetwork* network = GetCellularNetwork(service_path_);

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
  SetFontAndTextDirection(&strings);

  static const base::StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_MOBILE_SETUP_PAGE_HTML));

  const std::string full_html = jstemplate_builder::GetI18nTemplateHtml(
      html, &strings);

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
MobileSetupHandler::MobileSetupHandler(const std::string& service_path)
    : tab_contents_(NULL),
      state_(PLAN_ACTIVATION_PAGE_LOADING),
      service_path_(service_path),
      reenable_wifi_(false),
      reenable_ethernet_(false),
      reenable_cert_check_(false),
      evaluating_(false),
      already_running_(false),
      connection_retry_count_(0),
      reconnect_wait_count_(0),
      activation_attempt_(0) {
}

MobileSetupHandler::~MobileSetupHandler() {
  reconnect_timer_.Stop();
  chromeos::NetworkLibrary* lib =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  lib->RemoveNetworkManagerObserver(this);
  lib->RemoveObserverForAllNetworks(this);
  if (lib->IsLocked())
    lib->Unlock();
  ReEnableOtherConnections();
}

WebUIMessageHandler* MobileSetupHandler::Attach(WebUI* web_ui) {
  return WebUIMessageHandler::Attach(web_ui);
}

void MobileSetupHandler::Init(TabContents* contents) {
  tab_contents_ = contents;
  LoadCellularConfig();
  if (!chromeos::CrosLibrary::Get()->GetNetworkLibrary()->IsLocked())
    SetupActivationProcess(GetCellularNetwork(service_path_));
  else
    already_running_ = true;
}

void MobileSetupHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback(kJsApiStartActivation,
      NewCallback(this, &MobileSetupHandler::HandleStartActivation));
  web_ui_->RegisterMessageCallback(kJsApiSetTransactionStatus,
      NewCallback(this, &MobileSetupHandler::HandleSetTransactionStatus));
}

void MobileSetupHandler::OnNetworkManagerChanged(
    chromeos::NetworkLibrary* cros) {
  if (state_ == PLAN_ACTIVATION_PAGE_LOADING)
    return;
  // Note that even though we get here when the service has
  // reappeared after disappearing earlier in the activation
  // process, there's no need to re-establish the NetworkObserver,
  // because the service path remains the same.
  EvaluateCellularNetwork(GetCellularNetwork(service_path_));
}

void MobileSetupHandler::OnNetworkChanged(chromeos::NetworkLibrary* cros,
                                          const chromeos::Network* network) {
  if (state_ == PLAN_ACTIVATION_PAGE_LOADING)
    return;
  DCHECK(network && network->type() == chromeos::TYPE_CELLULAR);
  EvaluateCellularNetwork(
      static_cast<chromeos::CellularNetwork*>(
          const_cast<chromeos::Network*>(network)));
}

void MobileSetupHandler::HandleStartActivation(const ListValue* args) {
  InitiateActivation();
  UMA_HISTOGRAM_COUNTS("Cellular.MobileSetupStart", 1);
}

void MobileSetupHandler::HandleSetTransactionStatus(const ListValue* args) {
  const size_t kSetTransactionStatusParamCount = 1;
  if (args->GetSize() != kSetTransactionStatusParamCount)
    return;
  // Get change callback function name.
  std::string status;
  if (!args->GetString(0, &status))
    return;
  scoped_refptr<TaskProxy> task = new TaskProxy(AsWeakPtr(), status);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(task.get(), &TaskProxy::HandleSetTransactionStatus));
}

void MobileSetupHandler::InitiateActivation() {
  scoped_refptr<TaskProxy> task = new TaskProxy(AsWeakPtr(), 0);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(task.get(), &TaskProxy::HandleStartActivation));
}

void MobileSetupHandler::StartActivation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  chromeos::NetworkLibrary* lib =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  chromeos::CellularNetwork* network = GetCellularNetwork(service_path_);
  // Check if we can start activation process.
  if (!network || already_running_) {
    std::string error;
    if (already_running_)
      error = kErrorAlreadyRunning;
    else if (!lib->cellular_available())
      error = kErrorNoDevice;
    else if (!lib->cellular_enabled())
      error = kErrorDisabled;
    else
      error = kErrorNoService;
    ChangeState(NULL, PLAN_ACTIVATION_ERROR, GetErrorMessage(error));
    return;
  }

  // Start monitoring network property changes.
  lib->AddNetworkManagerObserver(this);
  lib->RemoveObserverForAllNetworks(this);
  lib->AddNetworkObserver(network->service_path(), this);
  // Try to start with OTASP immediately if we have received payment recently.
  state_ = lib->HasRecentCellularPlanPayment() ?
               PLAN_ACTIVATION_START_OTASP :
               PLAN_ACTIVATION_START;
  EvaluateCellularNetwork(network);
}

void MobileSetupHandler::RetryOTASP() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(state_ == PLAN_ACTIVATION_DELAY_OTASP);
  StartOTASP();
}

void MobileSetupHandler::ContinueConnecting(int delay) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  chromeos::CellularNetwork* network = GetCellularNetwork(service_path_);
  if (network && network->connecting_or_connected()) {
    EvaluateCellularNetwork(network);
  } else {
    ConnectToNetwork(network, delay);
  }
}

void MobileSetupHandler::SetTransactionStatus(const std::string& status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // The payment is received, try to reconnect and check the status all over
  // again.
  if (LowerCaseEqualsASCII(status, "ok") &&
      state_ == PLAN_ACTIVATION_SHOWING_PAYMENT) {
    chromeos::NetworkLibrary* lib =
        chromeos::CrosLibrary::Get()->GetNetworkLibrary();
    lib->SignalCellularPlanPayment();
    UMA_HISTOGRAM_COUNTS("Cellular.PaymentReceived", 1);
    StartOTASP();
  } else {
    UMA_HISTOGRAM_COUNTS("Cellular.PaymentFailed", 1);
  }
}

void MobileSetupHandler::StartOTASP() {
  state_ = PLAN_ACTIVATION_START_OTASP;
  chromeos::CellularNetwork* network = GetCellularNetwork();
  if (network &&
      network->connected() &&
      network->activation_state() == chromeos::ACTIVATION_STATE_ACTIVATED) {
    chromeos::CrosLibrary::Get()->GetNetworkLibrary()->
        DisconnectFromWirelessNetwork(network);
  } else {
    EvaluateCellularNetwork(network);
  }
}

void MobileSetupHandler::ReconnectTimerFired() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Permit network connection changes only in reconnecting states.
  if (state_ != PLAN_ACTIVATION_RECONNECTING_OTASP_TRY &&
      state_ != PLAN_ACTIVATION_RECONNECTING &&
      state_ != PLAN_ACTIVATION_RECONNECTING_OTASP)
    return;
  chromeos::CellularNetwork* network = GetCellularNetwork(service_path_);
  if (!network) {
    // No service, try again since this is probably just transient condition.
    LOG(WARNING) << "Service not present at reconnect attempt.";
  }
  EvaluateCellularNetwork(network);
}

void MobileSetupHandler::DisconnectFromNetwork(
    chromeos::CellularNetwork* network) {
  LOG(INFO) << "Disconnecting from " <<
      network->service_path().c_str();
  chromeos::CrosLibrary::Get()->GetNetworkLibrary()->
      DisconnectFromWirelessNetwork(network);
  // Disconnect will force networks to be reevaluated, so
  // we don't want to continue processing on this path anymore.
  evaluating_ = false;
}

bool MobileSetupHandler::NeedsReconnecting(chromeos::CellularNetwork* network,
                                           PlanActivationState* new_state,
                                           std::string* error_description) {
  if (!network->failed() && !ConnectionTimeout())
    return false;

  // Try to reconnect again if reconnect failed, or if for some
  // reasons we are still not connected after 45 seconds.
  int max_retries = (state_ == PLAN_ACTIVATION_RECONNECTING_OTASP) ?
                        kMaxConnectionRetryOTASP : kMaxConnectionRetry;
  if (connection_retry_count_ < max_retries) {
    UMA_HISTOGRAM_COUNTS("Cellular.ConnectionRetry", 1);
    ConnectToNetwork(network, kFailedReconnectDelayMS);
    return true;
  }
  // We simply can't connect anymore after all these tries.
  UMA_HISTOGRAM_COUNTS("Cellular.ConnectionFailed", 1);
  *new_state = PLAN_ACTIVATION_ERROR;
  *error_description = GetErrorMessage(kFailedConnectivity);
  return false;
}

bool MobileSetupHandler::ConnectToNetwork(chromeos::CellularNetwork* network,
                                          int delay) {
  if (network && network->connecting_or_connected())
    return true;
  // Permit network connection changes only in reconnecting states.
  if (state_ != PLAN_ACTIVATION_RECONNECTING_OTASP_TRY &&
      state_ != PLAN_ACTIVATION_RECONNECTING &&
      state_ != PLAN_ACTIVATION_RECONNECTING_OTASP)
    return false;
  if (network) {
    LOG(INFO) << "Connecting to " <<
        network->service_path().c_str();
  }
  connection_retry_count_++;
  connection_start_time_ = base::Time::Now();
  if (!network || !chromeos::CrosLibrary::Get()->GetNetworkLibrary()->
          ConnectToCellularNetwork(network)) {
    LOG(WARNING) << "Connect failed"
                 << network->service_path().c_str();
    // If we coudn't connect during reconnection phase, try to reconnect
    // with a delay (and try to reconnect if needed).
    scoped_refptr<TaskProxy> task = new TaskProxy(AsWeakPtr(),
                                                  delay);
    BrowserThread::PostDelayedTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(task.get(), &TaskProxy::ContinueConnecting),
        delay);
    return false;
  }
  return true;
}

void MobileSetupHandler::ForceReconnect(chromeos::CellularNetwork* network,
                                        int delay) {
  DCHECK(network);
  UMA_HISTOGRAM_COUNTS("Cellular.ActivationRetry", 1);
  // Reset reconnect metrics.
  connection_retry_count_ = 0;
  connection_start_time_ = base::Time();
  // First, disconnect...
  LOG(INFO) << "Disconnecting from " <<
      network->service_path().c_str();
  chromeos::CrosLibrary::Get()->GetNetworkLibrary()->
      DisconnectFromWirelessNetwork(network);
  // Check the network state 3s after we disconnect to make sure.
  scoped_refptr<TaskProxy> task = new TaskProxy(AsWeakPtr(),
                                                delay);
  BrowserThread::PostDelayedTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(task.get(), &TaskProxy::ContinueConnecting),
      delay);
}

bool MobileSetupHandler::ConnectionTimeout() {
  return (base::Time::Now() -
            connection_start_time_).InSeconds() > kConnectionTimeoutSeconds;
}

void MobileSetupHandler::EvaluateCellularNetwork(
    chromeos::CellularNetwork* network) {
  if (!web_ui_)
    return;

  PlanActivationState new_state = state_;
  if (!network) {
    LOG(WARNING) << "Cellular service lost";
    if (state_ == PLAN_ACTIVATION_RECONNECTING_OTASP_TRY ||
        state_ == PLAN_ACTIVATION_RECONNECTING ||
        state_ == PLAN_ACTIVATION_RECONNECTING_OTASP) {
      // This might be the legit case when service is lost after activation.
      // We need to make sure we force reconnection as soon as it shows up.
      LOG(INFO) << "Force service reconnection";
      connection_start_time_ = base::Time();
    }
    return;
  }

  // Prevent this method from being called if it is already on the stack.
  // This might happen on some state transitions (ie. connect, disconnect).
  if (evaluating_)
    return;
  evaluating_ = true;
  std::string error_description;

  LOG(WARNING) << "Cellular:\n  service=" << network->GetStateString().c_str()
      << "\n  ui=" << GetStateDescription(state_)
      << "\n  activation=" << network->GetActivationStateString().c_str()
      << "\n  connectivity="
      << network->GetConnectivityStateString().c_str()
      << "\n  error=" << network->GetErrorString().c_str()
      << "\n  setvice_path=" << network->service_path().c_str();
  switch (state_) {
    case PLAN_ACTIVATION_START: {
      switch (network->activation_state()) {
        case chromeos::ACTIVATION_STATE_ACTIVATED: {
          if (network->failed_or_disconnected()) {
            new_state = PLAN_ACTIVATION_RECONNECTING;
          } else if (network->connected()) {
            if (network->restricted_pool()) {
              new_state = PLAN_ACTIVATION_SHOWING_PAYMENT;
            } else {
              new_state = PLAN_ACTIVATION_DONE;
            }
          }
          break;
        }
        default: {
          if (network->failed_or_disconnected() ||
              network->connection_state() ==
                  chromeos::STATE_ACTIVATION_FAILURE) {
            new_state = (network->activation_state() ==
                         chromeos::ACTIVATION_STATE_PARTIALLY_ACTIVATED) ?
                            PLAN_ACTIVATION_TRYING_OTASP :
                            PLAN_ACTIVATION_INITIATING_ACTIVATION;
          } else if (network->connected()) {
            DisconnectFromNetwork(network);
            return;
          }
          break;
        }
      }
      break;
    }
    case PLAN_ACTIVATION_START_OTASP: {
      switch (network->activation_state()) {
        case chromeos::ACTIVATION_STATE_PARTIALLY_ACTIVATED: {
          if (network->failed_or_disconnected()) {
            new_state = PLAN_ACTIVATION_OTASP;
          } else if (network->connected()) {
            DisconnectFromNetwork(network);
            return;
          }
          break;
        }
        case chromeos::ACTIVATION_STATE_ACTIVATED:
          new_state = PLAN_ACTIVATION_RECONNECTING_OTASP;
          break;
        default: {
          LOG(WARNING) << "Unexpected activation state for device "
                       << network->service_path().c_str();
          break;
        }
      }
      break;
    }
    case PLAN_ACTIVATION_DELAY_OTASP:
      // Just ignore any changes until the OTASP retry timer kicks in.
      evaluating_ = false;
      return;
    case PLAN_ACTIVATION_INITIATING_ACTIVATION:
    case PLAN_ACTIVATION_OTASP:
    case PLAN_ACTIVATION_TRYING_OTASP: {
      switch (network->activation_state()) {
        case chromeos::ACTIVATION_STATE_ACTIVATED:
          if (network->failed_or_disconnected()) {
            new_state = GetNextReconnectState(state_);
          } else if (network->connected()) {
            if (network->restricted_pool()) {
              new_state = PLAN_ACTIVATION_SHOWING_PAYMENT;
            } else {
              new_state = PLAN_ACTIVATION_DONE;
            }
          }
          break;
        case chromeos::ACTIVATION_STATE_PARTIALLY_ACTIVATED:
          if (network->connected()) {
            if (network->restricted_pool())
              new_state = PLAN_ACTIVATION_SHOWING_PAYMENT;
          } else {
            new_state = GetNextReconnectState(state_);
          }
          break;
        case chromeos::ACTIVATION_STATE_NOT_ACTIVATED:
        case chromeos::ACTIVATION_STATE_ACTIVATING:
          // Wait in this state until activation state changes.
          break;
        default:
          break;
      }
      break;
    }
    case PLAN_ACTIVATION_RECONNECTING_OTASP_TRY:
    case PLAN_ACTIVATION_RECONNECTING: {
      if (network->connected()) {
        // Make sure other networks are not interfering with our detection of
        // restricted pool.
        DisableOtherNetworks();
        // Wait until the service shows up and gets activated.
        switch (network->activation_state()) {
          case chromeos::ACTIVATION_STATE_PARTIALLY_ACTIVATED:
          case chromeos::ACTIVATION_STATE_ACTIVATED:
            if (network->connectivity_state() ==
                         chromeos::CONN_STATE_NONE) {
              LOG(WARNING) << "No connectivity for device "
                           << network->service_path().c_str();
              // If we are connected but there is no connectivity at all,
              // restart the whole process again.
              if (activation_attempt_ < kMaxActivationAttempt) {
                activation_attempt_++;
                LOG(WARNING) << "Reconnect attempt #"
                             << activation_attempt_;
                ForceReconnect(network, kFailedReconnectDelayMS);
                evaluating_ = false;
                return;
              } else {
                new_state = PLAN_ACTIVATION_ERROR;
                UMA_HISTOGRAM_COUNTS("Cellular.ActivationRetryFailure", 1);
                error_description = GetErrorMessage(kFailedConnectivity);
              }
            } else if (network->connectivity_state() ==
                           chromeos::CONN_STATE_RESTRICTED) {
              // If we have already received payment, don't show the payment
              // page again. We should try to reconnect after some time instead.
              new_state = PLAN_ACTIVATION_SHOWING_PAYMENT;
            } else if (network->activation_state() ==
                           chromeos::ACTIVATION_STATE_ACTIVATED) {
              new_state = PLAN_ACTIVATION_DONE;
            }
            break;
          default:
            break;
        }
      } else if (NeedsReconnecting(network, &new_state, &error_description)) {
        evaluating_ = false;
        return;
      }
      break;
    }
    case PLAN_ACTIVATION_RECONNECTING_OTASP: {
      if (network->connected()) {
        // Make sure other networks are not interfering with our detection of
        // restricted pool.
        DisableOtherNetworks();
        // Wait until the service shows up and gets activated.
        switch (network->activation_state()) {
          case chromeos::ACTIVATION_STATE_PARTIALLY_ACTIVATED:
          case chromeos::ACTIVATION_STATE_ACTIVATED:
            if (network->connectivity_state() == chromeos::CONN_STATE_NONE ||
                network->connectivity_state() ==
                    chromeos::CONN_STATE_RESTRICTED) {
              LOG(WARNING) << "Still no connectivity after OTASP for device "
                           << network->service_path().c_str();
              // If we have already received payment, don't show the payment
              // page again. We should try to reconnect after some time instead.
              if (reconnect_wait_count_ < kMaxReconnectAttemptOTASP) {
                reconnect_wait_count_++;
                LOG(WARNING) << "OTASP reconnect attempt #"
                             << reconnect_wait_count_;
                ForceReconnect(network, kPostPaymentReconnectDelayMS);
                evaluating_ = false;
                return;
              } else {
                new_state = PLAN_ACTIVATION_ERROR;
                UMA_HISTOGRAM_COUNTS("Cellular.PostPaymentConnectFailure", 1);
                error_description = GetErrorMessage(kFailedConnectivity);
              }
            } else if (network->connectivity_state() ==
                           chromeos::CONN_STATE_UNRESTRICTED) {
              new_state = PLAN_ACTIVATION_DONE;
            }
            break;
          default:
            break;
        }
      } else if (NeedsReconnecting(network, &new_state, &error_description)) {
        evaluating_ = false;
        return;
      }
      break;
    }
    case PLAN_ACTIVATION_PAGE_LOADING:
      break;
    // Just ignore all signals until the site confirms payment.
    case PLAN_ACTIVATION_SHOWING_PAYMENT:
      // Activation completed/failed, ignore network changes.
    case PLAN_ACTIVATION_DONE:
    case PLAN_ACTIVATION_ERROR:
      break;
  }

  if (new_state != PLAN_ACTIVATION_ERROR &&
      GotActivationError(network, &error_description)) {
    // Check for this special case when we try to do activate partially
    // activated device. If that attempt failed, try to disconnect to clear the
    // state and reconnect again.
    if ((network->activation_state() ==
            chromeos::ACTIVATION_STATE_PARTIALLY_ACTIVATED ||
        network->activation_state() == chromeos::ACTIVATION_STATE_ACTIVATING) &&
        (network->error() == chromeos::ERROR_UNKNOWN ||
            network->error() == chromeos::ERROR_OTASP_FAILED) &&
        network->connection_state() == chromeos::STATE_ACTIVATION_FAILURE) {
      LOG(WARNING) << "Activation failure detected "
                   << network->service_path().c_str();
      switch (state_) {
        case PLAN_ACTIVATION_OTASP:
        case PLAN_ACTIVATION_RECONNECTING_OTASP:
          new_state = PLAN_ACTIVATION_DELAY_OTASP;
          break;
        case PLAN_ACTIVATION_TRYING_OTASP:
          new_state = PLAN_ACTIVATION_RECONNECTING_OTASP_TRY;
          break;
        case PLAN_ACTIVATION_INITIATING_ACTIVATION:
          new_state = PLAN_ACTIVATION_RECONNECTING;
          break;
        case PLAN_ACTIVATION_START:
          // We are just starting, so this must be previous activation attempt
          // failure.
          new_state = PLAN_ACTIVATION_TRYING_OTASP;
          break;
        case PLAN_ACTIVATION_DELAY_OTASP:
        case PLAN_ACTIVATION_RECONNECTING_OTASP_TRY:
        case PLAN_ACTIVATION_RECONNECTING:
          new_state = state_;
          break;
        default:
          new_state = PLAN_ACTIVATION_ERROR;
          break;
      }
    } else {
      LOG(WARNING) << "Unexpected activation failure for "
                   << network->service_path().c_str();
      new_state = PLAN_ACTIVATION_ERROR;
    }
  }

  if (new_state == PLAN_ACTIVATION_ERROR && !error_description.length())
    error_description = GetErrorMessage(kErrorDefault);

  ChangeState(network, new_state, error_description);
  evaluating_ = false;
}

MobileSetupHandler::PlanActivationState
    MobileSetupHandler::GetNextReconnectState(
        MobileSetupHandler::PlanActivationState state) {
  switch (state) {
    case PLAN_ACTIVATION_INITIATING_ACTIVATION:
      return PLAN_ACTIVATION_RECONNECTING;
    case PLAN_ACTIVATION_OTASP:
      return PLAN_ACTIVATION_RECONNECTING_OTASP;
    case PLAN_ACTIVATION_TRYING_OTASP:
      return PLAN_ACTIVATION_RECONNECTING_OTASP_TRY;
    default:
      return PLAN_ACTIVATION_RECONNECTING;
  }
}

// Debugging helper function, will take it out at the end.
const char* MobileSetupHandler::GetStateDescription(
    PlanActivationState state) {
  switch (state) {
    case PLAN_ACTIVATION_PAGE_LOADING:
      return "PAGE_LOADING";
    case PLAN_ACTIVATION_START:
      return "ACTIVATION_START";
    case PLAN_ACTIVATION_TRYING_OTASP:
      return "TRYING_OTASP";
    case PLAN_ACTIVATION_RECONNECTING_OTASP_TRY:
      return "RECONNECTING_OTASP_TRY";
    case PLAN_ACTIVATION_INITIATING_ACTIVATION:
      return "INITIATING_ACTIVATION";
    case PLAN_ACTIVATION_RECONNECTING:
      return "RECONNECTING";
    case PLAN_ACTIVATION_SHOWING_PAYMENT:
      return "SHOWING_PAYMENT";
    case PLAN_ACTIVATION_START_OTASP:
      return "START_OTASP";
    case PLAN_ACTIVATION_DELAY_OTASP:
      return "DELAY_OTASP";
    case PLAN_ACTIVATION_OTASP:
      return "OTASP";
    case PLAN_ACTIVATION_RECONNECTING_OTASP:
      return "RECONNECTING_OTASP";
    case PLAN_ACTIVATION_DONE:
      return "DONE";
    case PLAN_ACTIVATION_ERROR:
      return "ERROR";
  }
  return "UNKNOWN";
}


void MobileSetupHandler::CompleteActivation(
    chromeos::CellularNetwork* network) {
  // Remove observers, we are done with this page.
  chromeos::NetworkLibrary* lib = chromeos::CrosLibrary::Get()->
      GetNetworkLibrary();
  lib->RemoveNetworkManagerObserver(this);
  lib->RemoveObserverForAllNetworks(this);
  if (lib->IsLocked())
    lib->Unlock();
  // If we have successfully activated the connection, set autoconnect flag.
  if (network) {
    network->set_auto_connect(true);
    lib->SaveCellularNetwork(network);
  }
  // Reactivate other types of connections if we have
  // shut them down previously.
  ReEnableOtherConnections();
}

void MobileSetupHandler::UpdatePage(chromeos::CellularNetwork* network,
                                    const std::string& error_description) {
  DictionaryValue device_dict;
  if (network)
    GetDeviceInfo(network, &device_dict);
  device_dict.SetInteger("state", state_);
  if (error_description.length())
    device_dict.SetString("error", error_description);
  web_ui_->CallJavascriptFunction(
      kJsDeviceStatusChangedHandler, device_dict);
}


void MobileSetupHandler::ChangeState(chromeos::CellularNetwork* network,
                                     PlanActivationState new_state,
                                     const std::string& error_description) {
  static bool first_time = true;
  if (state_ == new_state && !first_time)
    return;
  LOG(WARNING) << "Activation state flip old = "
      << GetStateDescription(state_)
      << ", new = " << GetStateDescription(new_state);
  first_time = false;

  // Pick action that should happen on leaving the old state.
  switch (state_) {
    case PLAN_ACTIVATION_RECONNECTING_OTASP_TRY:
    case PLAN_ACTIVATION_RECONNECTING:
    case PLAN_ACTIVATION_RECONNECTING_OTASP:
      if (reconnect_timer_.IsRunning()) {
        reconnect_timer_.Stop();
      }
      break;
    default:
      break;
  }
  state_ = new_state;

  // Signal to JS layer that the state is changing.
  UpdatePage(network, error_description);

  // Pick action that should happen on entering the new state.
  switch (new_state) {
    case PLAN_ACTIVATION_START:
      break;
    case PLAN_ACTIVATION_DELAY_OTASP: {
      UMA_HISTOGRAM_COUNTS("Cellular.RetryOTASP", 1);
      scoped_refptr<TaskProxy> task = new TaskProxy(AsWeakPtr(), 0);
      BrowserThread::PostDelayedTask(BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(task.get(), &TaskProxy::RetryOTASP),
          kOTASPRetryDelay);
      break;
    }
    case PLAN_ACTIVATION_INITIATING_ACTIVATION:
    case PLAN_ACTIVATION_TRYING_OTASP:
    case PLAN_ACTIVATION_OTASP:
      DCHECK(network);
      LOG(WARNING) << "Activating service " << network->service_path().c_str();
      UMA_HISTOGRAM_COUNTS("Cellular.ActivationTry", 1);
      if (!network->StartActivation()) {
        UMA_HISTOGRAM_COUNTS("Cellular.ActivationFailure", 1);
        if (new_state == PLAN_ACTIVATION_OTASP) {
          ChangeState(network, PLAN_ACTIVATION_DELAY_OTASP, std::string());
        } else {
          ChangeState(network, PLAN_ACTIVATION_ERROR,
                      GetErrorMessage(kFailedConnectivity));
        }
      }
      break;
    case PLAN_ACTIVATION_RECONNECTING_OTASP_TRY:
    case PLAN_ACTIVATION_RECONNECTING:
    case PLAN_ACTIVATION_RECONNECTING_OTASP: {
      // Start reconnect timer. This will ensure that we are not left in
      // limbo by the network library.
      if (!reconnect_timer_.IsRunning()) {
        reconnect_timer_.Start(
            base::TimeDelta::FromMilliseconds(kReconnectTimerDelayMS),
            this, &MobileSetupHandler::ReconnectTimerFired);
      }
      // Reset connection metrics and try to connect.
      reconnect_wait_count_ = 0;
      connection_retry_count_ = 0;
      connection_start_time_ = base::Time::Now();
      ConnectToNetwork(network, kReconnectDelayMS);
      break;
    }
    case PLAN_ACTIVATION_PAGE_LOADING:
      return;
    case PLAN_ACTIVATION_SHOWING_PAYMENT:
      // Fix for fix SSL for the walled gardens where cert chain verification
      // might not work.
      break;
    case PLAN_ACTIVATION_DONE:
      DCHECK(network);
      CompleteActivation(network);
      UMA_HISTOGRAM_COUNTS("Cellular.MobileSetupSucceeded", 1);
      break;
    case PLAN_ACTIVATION_ERROR:
      CompleteActivation(NULL);
      UMA_HISTOGRAM_COUNTS("Cellular.PlanFailed", 1);
      break;
    default:
      break;
  }
}

void MobileSetupHandler::ReEnableOtherConnections() {
  chromeos::NetworkLibrary* lib = chromeos::CrosLibrary::Get()->
      GetNetworkLibrary();
  if (reenable_ethernet_) {
    reenable_ethernet_ = false;
    lib->EnableEthernetNetworkDevice(true);
  }
  if (reenable_wifi_) {
    reenable_wifi_ = false;
    lib->EnableWifiNetworkDevice(true);
  }

  PrefService* prefs = web_ui_->GetProfile()->GetPrefs();
  if (reenable_cert_check_) {
    prefs->SetBoolean(prefs::kCertRevocationCheckingEnabled,
                      true);
    reenable_cert_check_ = false;
  }
}

void MobileSetupHandler::SetupActivationProcess(
    chromeos::CellularNetwork* network) {
  if (!network)
    return;

  // Disable SSL cert checks since we will be doing this in
  // restricted pool.
  PrefService* prefs = web_ui_->GetProfile()->GetPrefs();
  if (!reenable_cert_check_ &&
      prefs->GetBoolean(
          prefs::kCertRevocationCheckingEnabled)) {
    reenable_cert_check_ = true;
    prefs->SetBoolean(prefs::kCertRevocationCheckingEnabled, false);
  }

  chromeos::NetworkLibrary* lib = chromeos::CrosLibrary::Get()->
      GetNetworkLibrary();
  // Disable autoconnect to cellular network.
  network->set_auto_connect(false);
  lib->SaveCellularNetwork(network);

  // Prevent any other network interference.
  DisableOtherNetworks();
  lib->Lock();
}

void MobileSetupHandler::DisableOtherNetworks() {
  chromeos::NetworkLibrary* lib = chromeos::CrosLibrary::Get()->
      GetNetworkLibrary();
  // Disable ethernet and wifi.
  if (lib->ethernet_enabled()) {
    reenable_ethernet_ = true;
    lib->EnableEthernetNetworkDevice(false);
  }
  if (lib->wifi_enabled()) {
    reenable_wifi_ = true;
    lib->EnableWifiNetworkDevice(false);
  }
}

bool MobileSetupHandler::GotActivationError(
    const chromeos::CellularNetwork* network, std::string* error) {
  DCHECK(network);
  bool got_error = false;
  const char* error_code = kErrorDefault;

  // This is the magic for detection of errors in during activation process.
  if (network->connection_state() == chromeos::STATE_FAILURE &&
      network->error() == chromeos::ERROR_AAA_FAILED) {
    if (network->activation_state() ==
            chromeos::ACTIVATION_STATE_PARTIALLY_ACTIVATED) {
      error_code = kErrorBadConnectionPartial;
    } else if (network->activation_state() ==
            chromeos::ACTIVATION_STATE_ACTIVATED) {
      if (network->roaming_state() == chromeos::ROAMING_STATE_HOME) {
        error_code = kErrorBadConnectionActivated;
      } else if (network->roaming_state() == chromeos::ROAMING_STATE_ROAMING) {
        error_code = kErrorRoamingOnConnection;
      }
    }
    got_error = true;
  } else if (network->connection_state() ==
                 chromeos::STATE_ACTIVATION_FAILURE) {
    if (network->error() == chromeos::ERROR_NEED_EVDO) {
      if (network->activation_state() ==
              chromeos::ACTIVATION_STATE_PARTIALLY_ACTIVATED)
        error_code = kErrorNoEVDO;
    } else if (network->error() == chromeos::ERROR_NEED_HOME_NETWORK) {
      if (network->activation_state() ==
              chromeos::ACTIVATION_STATE_NOT_ACTIVATED) {
        error_code = kErrorRoamingActivation;
      } else if (network->activation_state() ==
                    chromeos::ACTIVATION_STATE_PARTIALLY_ACTIVATED) {
        error_code = kErrorRoamingPartiallyActivated;
      }
    }
    got_error = true;
  }

  if (got_error)
    *error = GetErrorMessage(error_code);

  return got_error;
}

void MobileSetupHandler::GetDeviceInfo(const chromeos::CellularNetwork* network,
          DictionaryValue* value) {
  value->SetString("carrier", network->name());
  value->SetString("payment_url", network->payment_url());
  value->SetString("MEID", network->meid());
  value->SetString("IMEI", network->imei());
  value->SetString("MDN", network->mdn());
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
  bool config_exists = false;
  {
    // Reading config file causes us to do blocking IO on UI thread.
    // Temporarily allow it until we fix http://crosbug.com/11535
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    config_exists = file_util::PathExists(config_path);
  }
  if (config_exists) {
    scoped_ptr<CellularConfigDocument> config(new CellularConfigDocument());
    bool config_loaded = config->LoadFromFile(config_path);
    if (config_loaded) {
      LOG(INFO) << "Cellular config file loaded: " << kCellularConfigPath;
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

MobileSetupUI::MobileSetupUI(TabContents* contents) : WebUI(contents) {
  const chromeos::CellularNetwork* network = GetCellularNetwork();
  std::string service_path = network ? network->service_path() : std::string();
  MobileSetupHandler* handler = new MobileSetupHandler(service_path);
  AddMessageHandler((handler)->Attach(this));
  handler->Init(contents);
  MobileSetupUIHTMLSource* html_source =
      new MobileSetupUIHTMLSource(service_path);

  // Set up the chrome://mobilesetup/ source.
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);
}
