// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/sim_unlock_ui.h"

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chrome/browser/chromeos/sim_dialog_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

using content::BrowserThread;
using content::WebContents;
using content::WebUIMessageHandler;

namespace {

// JS API callbacks names.
const char kJsApiChangePinCode[] = "changePinCode";
const char kJsApiEnterPinCode[] = "enterPinCode";
const char kJsApiEnterPukCode[] = "enterPukCode";
const char kJsApiProceedToPukInput[] = "proceedToPukInput";
const char kJsApiSimStatusInitialize[] = "simStatusInitialize";

// Page JS API function names.
const char kJsApiSimStatusChanged[] = "mobile.SimUnlock.simStateChanged";

// SIM state variables which are passed to the page.
const char kState[] = "state";
const char kError[] = "error";
const char kTriesLeft[] = "tries";

// Error constants, passed to the page.
const char kErrorPin[] = "incorrectPin";
const char kErrorOk[] = "ok";

chromeos::NetworkDeviceHandler* GetNetworkDeviceHandler() {
  return chromeos::NetworkHandler::Get()->network_device_handler();
}

chromeos::NetworkStateHandler* GetNetworkStateHandler() {
  return chromeos::NetworkHandler::Get()->network_state_handler();
}

}  // namespace

namespace chromeos {

class SimUnlockUIHTMLSource : public content::URLDataSource {
 public:
  SimUnlockUIHTMLSource();

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
  virtual ~SimUnlockUIHTMLSource() {}

  std::string service_path_;
  DISALLOW_COPY_AND_ASSIGN(SimUnlockUIHTMLSource);
};

// The handler for Javascript messages related to the "sim-unlock" view.
class SimUnlockHandler : public WebUIMessageHandler,
                         public base::SupportsWeakPtr<SimUnlockHandler>,
                         public NetworkStateHandlerObserver {
 public:
  SimUnlockHandler();
  virtual ~SimUnlockHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // NetworkStateHandlerObserver implementation.
  virtual void DeviceListChanged() OVERRIDE;

 private:
  // Should keep this state enum in sync with similar one in JS code.
  // SIM_NOT_LOCKED_ASK_PIN - SIM card is not locked but we ask user
  // for PIN input because PinRequired preference change was requested.
  // SIM_NOT_LOCKED_CHANGE_PIN - SIM card is not locked, ask user for old PIN
  // and new PIN to change it.
  typedef enum SimUnlockState {
    SIM_UNLOCK_LOADING           = -1,
    SIM_ABSENT_NOT_LOCKED        =  0,
    SIM_NOT_LOCKED_ASK_PIN       =  1,
    SIM_NOT_LOCKED_CHANGE_PIN    =  2,
    SIM_LOCKED_PIN               =  3,
    SIM_LOCKED_NO_PIN_TRIES_LEFT =  4,
    SIM_LOCKED_PUK               =  5,
    SIM_LOCKED_NO_PUK_TRIES_LEFT =  6,
    SIM_DISABLED                 =  7,
  } SimUnlockState;

  // Type of the SIM unlock code.
  enum SimUnlockCode {
    CODE_PIN,
    CODE_PUK
  };

  enum PinOperationError {
    PIN_ERROR_NONE = 0,
    PIN_ERROR_UNKNOWN = 1,
    PIN_ERROR_INCORRECT_CODE = 2,
    PIN_ERROR_BLOCKED = 3
  };

  class TaskProxy : public base::RefCountedThreadSafe<TaskProxy> {
   public:
    explicit TaskProxy(const base::WeakPtr<SimUnlockHandler>& handler)
        : handler_(handler),
          code_type_() {
    }

    TaskProxy(const base::WeakPtr<SimUnlockHandler>& handler,
              const std::string& code,
              SimUnlockCode code_type)
        : handler_(handler),
          code_(code),
          code_type_(code_type) {
    }

    void HandleEnterCode() {
      if (handler_)
        handler_->EnterCode(code_, code_type_);
    }

    void HandleInitialize() {
      if (handler_)
        handler_->InitializeSimStatus();
    }

    void HandleProceedToPukInput() {
      if (handler_)
        handler_->ProceedToPukInput();
    }

   private:
    friend class base::RefCountedThreadSafe<TaskProxy>;

    ~TaskProxy() {}

    base::WeakPtr<SimUnlockHandler> handler_;

    // Pending code input (PIN/PUK).
    std::string code_;

    // Pending code type.
    SimUnlockCode code_type_;

    DISALLOW_COPY_AND_ASSIGN(TaskProxy);
  };

  // Returns the cellular device that this dialog currently corresponds to.
  const DeviceState* GetCellularDevice();

  // Pass PIN/PUK code to shill and check status.
  void EnterCode(const std::string& code, SimUnlockCode code_type);

  // Methods to invoke shill PIN/PUK D-Bus operations.
  void ChangeRequirePin(bool require_pin, const std::string& pin);
  void EnterPin(const std::string& pin);
  void ChangePin(const std::string& old_pin, const std::string& new_pin);
  void UnblockPin(const std::string& puk, const std::string& new_pin);
  void PinOperationSuccessCallback(const std::string& operation_name);
  void PinOperationErrorCallback(const std::string& operation_name,
                                 const std::string& error_name,
                                 scoped_ptr<base::DictionaryValue> error_data);

  // Called when an asynchronous PIN operation has completed.
  void OnPinOperationCompleted(PinOperationError error);

  // Single handler for PIN/PUK code operations.
  void HandleEnterCode(SimUnlockCode code_type, const std::string& code);

  // Handlers for JS WebUI messages.
  void HandleChangePinCode(const base::ListValue* args);
  void HandleEnterPinCode(const base::ListValue* args);
  void HandleEnterPukCode(const base::ListValue* args);
  void HandleProceedToPukInput(const base::ListValue* args);
  void HandleSimStatusInitialize(const base::ListValue* args);

  // Initialize current SIM card status, passes that to page.
  void InitializeSimStatus();

  // Checks whether SIM card is in PUK locked state and proceeds to PUK input.
  void ProceedToPukInput();

  // Processes current SIM card state and update internal state/page.
  void ProcessSimCardState(const DeviceState* cellular);

  // Updates page with the current state/SIM card info/error.
  void UpdatePage(const DeviceState* cellular, const std::string& error_msg);

  // Dialog internal state.
  SimUnlockState state_;

  // Path of the Cellular device that we monitor property updates from.
  std::string cellular_device_path_;

  // Type of the dialog: generic unlock/change pin/change PinRequire.
  SimDialogDelegate::SimDialogMode dialog_mode_;

  // New PIN value for the case when we unblock SIM card or change PIN.
  std::string new_pin_;

  // The initial lock type value, used to observe changes to lock status;
  std::string sim_lock_type_;

  // True if there's a pending PIN operation.
  // That means that SIM lock state change will be received 2 times:
  // OnNetworkDeviceSimLockChanged and OnPinOperationCompleted.
  // First one should be ignored.
  bool pending_pin_operation_;

  base::WeakPtrFactory<SimUnlockHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SimUnlockHandler);
};

// SimUnlockUIHTMLSource -------------------------------------------------------

SimUnlockUIHTMLSource::SimUnlockUIHTMLSource() {
}

std::string SimUnlockUIHTMLSource::GetSource() const {
  return chrome::kChromeUISimUnlockHost;
}

void SimUnlockUIHTMLSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  base::DictionaryValue strings;
  strings.SetString("title",
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_ENTER_PIN_TITLE));
  strings.SetString("ok", l10n_util::GetStringUTF16(IDS_OK));
  strings.SetString("cancel", l10n_util::GetStringUTF16(IDS_CANCEL));
  strings.SetString("enterPinTitle",
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_ENTER_PIN_TITLE));
  strings.SetString("enterPinMessage",
      l10n_util::GetStringUTF16(IDS_SIM_ENTER_PIN_MESSAGE));
  strings.SetString("enterPinTriesMessage",
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_ENTER_PIN_TRIES_MESSAGE));
  strings.SetString("incorrectPinTriesMessage",
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_INCORRECT_PIN_TRIES_MESSAGE));
  strings.SetString("incorrectPinTitle",
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_INCORRECT_PIN_TITLE));
  // TODO(nkostylev): Pass carrier name if we know that.
  strings.SetString("noPinTriesLeft", l10n_util::GetStringFUTF16(
      IDS_SIM_UNLOCK_NO_PIN_TRIES_LEFT_MESSAGE,
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_DEFAULT_CARRIER)));
  strings.SetString("enterPukButton",
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_ENTER_PUK_BUTTON));
  strings.SetString("enterPukTitle",
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_ENTER_PUK_TITLE));
  strings.SetString("enterPukWarning",
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_ENTER_PUK_WARNING));
  // TODO(nkostylev): Pass carrier name if we know that.
  strings.SetString("enterPukMessage", l10n_util::GetStringFUTF16(
      IDS_SIM_UNLOCK_ENTER_PUK_MESSAGE,
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_DEFAULT_CARRIER)));
  strings.SetString("choosePinTitle",
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_CHOOSE_PIN_TITLE));
  strings.SetString("choosePinMessage",
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_CHOOSE_PIN_MESSAGE));
  strings.SetString("newPin", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_CHANGE_PIN_NEW_PIN));
  strings.SetString("retypeNewPin", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_CHANGE_PIN_RETYPE_PIN));
  strings.SetString("pinsDontMatchMessage", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_PINS_DONT_MATCH_ERROR));
  strings.SetString("noPukTriesLeft",
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_NO_PUK_TRIES_LEFT_MESSAGE));
  strings.SetString("simDisabledTitle",
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_SIM_DISABLED_TITLE));
  strings.SetString("simDisabledMessage",
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_SIM_DISABLED_MESSAGE));

  strings.SetString("changePinTitle", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_CHANGE_PIN_TITLE));
  strings.SetString("changePinMessage", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_CHANGE_PIN_MESSAGE));
  strings.SetString("oldPin", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_CHANGE_PIN_OLD_PIN));

  webui::SetFontAndTextDirection(&strings);

  static const base::StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_SIM_UNLOCK_HTML));

  webui::UseVersion2 version2;
  std::string full_html = webui::GetI18nTemplateHtml(html, &strings);

  callback.Run(base::RefCountedString::TakeString(&full_html));
}

// SimUnlockHandler ------------------------------------------------------------

SimUnlockHandler::SimUnlockHandler()
    : state_(SIM_UNLOCK_LOADING),
      dialog_mode_(SimDialogDelegate::SIM_DIALOG_UNLOCK),
      pending_pin_operation_(false),
      weak_ptr_factory_(this) {
  if (GetNetworkStateHandler()
          ->GetTechnologyState(NetworkTypePattern::Cellular()) !=
      NetworkStateHandler::TECHNOLOGY_UNAVAILABLE)
    GetNetworkStateHandler()->AddObserver(this, FROM_HERE);
}

SimUnlockHandler::~SimUnlockHandler() {
  GetNetworkStateHandler()->RemoveObserver(this, FROM_HERE);
}

void SimUnlockHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(kJsApiChangePinCode,
      base::Bind(&SimUnlockHandler::HandleChangePinCode,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kJsApiEnterPinCode,
      base::Bind(&SimUnlockHandler::HandleEnterPinCode,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kJsApiEnterPukCode,
      base::Bind(&SimUnlockHandler::HandleEnterPukCode,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kJsApiProceedToPukInput,
      base::Bind(&SimUnlockHandler::HandleProceedToPukInput,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kJsApiSimStatusInitialize,
      base::Bind(&SimUnlockHandler::HandleSimStatusInitialize,
                 base::Unretained(this)));
}

void SimUnlockHandler::DeviceListChanged() {
  const DeviceState* cellular_device = GetCellularDevice();
  if (!cellular_device) {
    LOG(WARNING) << "Cellular device with path '" << cellular_device_path_
                 << "' disappeared.";
    ProcessSimCardState(NULL);
    return;
  }

  // Process the SIM card state only if the lock state changed.
  if (cellular_device->sim_lock_type() == sim_lock_type_)
    return;

  sim_lock_type_ = cellular_device->sim_lock_type();
  uint32 retries_left = cellular_device->sim_retries_left();
  VLOG(1) << "OnNetworkDeviceSimLockChanged, lock: " << sim_lock_type_
          << ", retries: " << retries_left;
  // There's a pending PIN operation.
  // Wait for it to finish and refresh state then.
  if (!pending_pin_operation_)
    ProcessSimCardState(cellular_device);
}

void SimUnlockHandler::OnPinOperationCompleted(PinOperationError error) {
  pending_pin_operation_ = false;
  VLOG(1) << "OnPinOperationCompleted, error: " << error;
  const DeviceState* cellular = GetCellularDevice();
  if (!cellular) {
    VLOG(1) << "Cellular device disappeared. Dismissing dialog.";
    ProcessSimCardState(NULL);
    return;
  }
  if (state_ == SIM_NOT_LOCKED_ASK_PIN && error == PIN_ERROR_NONE) {
    CHECK(dialog_mode_ == SimDialogDelegate::SIM_DIALOG_SET_LOCK_ON ||
          dialog_mode_ == SimDialogDelegate::SIM_DIALOG_SET_LOCK_OFF);
    // Dialog will close itself.
    state_ = SIM_ABSENT_NOT_LOCKED;
  } else if (state_ == SIM_NOT_LOCKED_CHANGE_PIN && error == PIN_ERROR_NONE) {
    CHECK(dialog_mode_ == SimDialogDelegate::SIM_DIALOG_CHANGE_PIN);
    // Dialog will close itself.
    state_ = SIM_ABSENT_NOT_LOCKED;
  }
  // If previous EnterPIN was last PIN attempt and SIMLock state was already
  // processed by OnNetworkDeviceChanged, let dialog stay on
  // NO_PIN_RETRIES_LEFT step.
  if (!(state_ == SIM_LOCKED_NO_PIN_TRIES_LEFT && error == PIN_ERROR_BLOCKED))
    ProcessSimCardState(cellular);
}

const DeviceState* SimUnlockHandler::GetCellularDevice() {
  return GetNetworkStateHandler()->GetDeviceState(cellular_device_path_);
}

void SimUnlockHandler::EnterCode(const std::string& code,
                                 SimUnlockCode code_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  pending_pin_operation_ = true;

  switch (code_type) {
    case CODE_PIN:
      if (dialog_mode_ == SimDialogDelegate::SIM_DIALOG_SET_LOCK_ON ||
          dialog_mode_ == SimDialogDelegate::SIM_DIALOG_SET_LOCK_OFF) {
        if (!sim_lock_type_.empty()) {
          // If SIM is locked/absent, change RequirePin UI is not accessible.
          NOTREACHED() <<
              "Changing RequirePin pref on locked / uninitialized SIM.";
        }
        ChangeRequirePin(
            dialog_mode_ == SimDialogDelegate::SIM_DIALOG_SET_LOCK_ON,
            code);
      } else if (dialog_mode_ == SimDialogDelegate::SIM_DIALOG_CHANGE_PIN) {
        if (!sim_lock_type_.empty()) {
          // If SIM is locked/absent, changing PIN UI is not accessible.
          NOTREACHED() << "Changing PIN on locked / uninitialized SIM.";
        }
        ChangePin(code, new_pin_);
      } else {
        EnterPin(code);
      }
      break;
    case CODE_PUK:
      DCHECK(!new_pin_.empty());
      UnblockPin(code, new_pin_);
      break;
  }
}

void SimUnlockHandler::ChangeRequirePin(bool require_pin,
                                        const std::string& pin) {
  const DeviceState* cellular = GetCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling RequirePin method w/o cellular device.";
    return;
  }
  std::string operation_name = "ChangeRequirePin";
  NET_LOG_USER(operation_name, cellular->path());
  GetNetworkDeviceHandler()->RequirePin(
      cellular->path(),
      require_pin,
      pin,
      base::Bind(&SimUnlockHandler::PinOperationSuccessCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 operation_name),
      base::Bind(&SimUnlockHandler::PinOperationErrorCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 operation_name));
}

void SimUnlockHandler::EnterPin(const std::string& pin) {
  const DeviceState* cellular = GetCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling RequirePin method w/o cellular device.";
    return;
  }
  std::string operation_name = "EnterPin";
  NET_LOG_USER(operation_name, cellular->path());
  GetNetworkDeviceHandler()->EnterPin(
      cellular->path(),
      pin,
      base::Bind(&SimUnlockHandler::PinOperationSuccessCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 operation_name),
      base::Bind(&SimUnlockHandler::PinOperationErrorCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 operation_name));
}

void SimUnlockHandler::ChangePin(const std::string& old_pin,
                                 const std::string& new_pin) {
  const DeviceState* cellular = GetCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling RequirePin method w/o cellular device.";
    return;
  }
  std::string operation_name = "ChangePin";
  NET_LOG_USER(operation_name, cellular->path());
  GetNetworkDeviceHandler()->ChangePin(
      cellular->path(),
      old_pin,
      new_pin,
      base::Bind(&SimUnlockHandler::PinOperationSuccessCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 operation_name),
      base::Bind(&SimUnlockHandler::PinOperationErrorCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 operation_name));
}

void SimUnlockHandler::UnblockPin(const std::string& puk,
                                  const std::string& new_pin) {
  const DeviceState* cellular = GetCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling RequirePin method w/o cellular device.";
    return;
  }
  std::string operation_name = "UnblockPin";
  NET_LOG_USER(operation_name, cellular->path());
  GetNetworkDeviceHandler()->UnblockPin(
      cellular->path(),
      puk,
      new_pin,
      base::Bind(&SimUnlockHandler::PinOperationSuccessCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 operation_name),
      base::Bind(&SimUnlockHandler::PinOperationErrorCallback,
                 weak_ptr_factory_.GetWeakPtr(),
                 operation_name));
}

void SimUnlockHandler::PinOperationSuccessCallback(
    const std::string& operation_name) {
  NET_LOG_DEBUG("Pin operation successful.", operation_name);
  OnPinOperationCompleted(PIN_ERROR_NONE);
}

void SimUnlockHandler::PinOperationErrorCallback(
    const std::string& operation_name,
    const std::string& error_name,
    scoped_ptr<base::DictionaryValue> error_data) {
  NET_LOG_ERROR("Pin operation failed: " + error_name, operation_name);
  PinOperationError pin_error;
  if (error_name == NetworkDeviceHandler::kErrorIncorrectPin ||
      error_name == NetworkDeviceHandler::kErrorPinRequired)
    pin_error = PIN_ERROR_INCORRECT_CODE;
  else if (error_name == NetworkDeviceHandler::kErrorPinBlocked)
    pin_error = PIN_ERROR_BLOCKED;
  else
    pin_error = PIN_ERROR_UNKNOWN;
  OnPinOperationCompleted(pin_error);
}

void SimUnlockHandler::HandleChangePinCode(const base::ListValue* args) {
  const size_t kChangePinParamCount = 2;
  std::string pin;
  std::string new_pin;
  if (args->GetSize() != kChangePinParamCount ||
      !args->GetString(0, &pin) ||
      !args->GetString(1, &new_pin)) {
    NOTREACHED();
    return;
  }
  new_pin_ = new_pin;
  HandleEnterCode(CODE_PIN, pin);
}

void SimUnlockHandler::HandleEnterCode(SimUnlockCode code_type,
                                       const std::string& code) {
  scoped_refptr<TaskProxy> task = new TaskProxy(AsWeakPtr(), code, code_type);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&TaskProxy::HandleEnterCode, task.get()));
}

void SimUnlockHandler::HandleEnterPinCode(const base::ListValue* args) {
  const size_t kEnterPinParamCount = 1;
  std::string pin;
  if (args->GetSize() != kEnterPinParamCount || !args->GetString(0, &pin)) {
    NOTREACHED();
    return;
  }
  HandleEnterCode(CODE_PIN, pin);
}

void SimUnlockHandler::HandleEnterPukCode(const base::ListValue* args) {
  const size_t kEnterPukParamCount = 2;
  std::string puk;
  std::string new_pin;
  if (args->GetSize() != kEnterPukParamCount ||
      !args->GetString(0, &puk) ||
      !args->GetString(1, &new_pin)) {
    NOTREACHED();
    return;
  }
  new_pin_ = new_pin;
  HandleEnterCode(CODE_PUK, puk);
}

void SimUnlockHandler::HandleProceedToPukInput(const base::ListValue* args) {
  const size_t kProceedToPukInputParamCount = 0;
  if (args->GetSize() != kProceedToPukInputParamCount) {
    NOTREACHED();
    return;
  }
  scoped_refptr<TaskProxy> task = new TaskProxy(AsWeakPtr());
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&TaskProxy::HandleProceedToPukInput, task.get()));
}

void SimUnlockHandler::HandleSimStatusInitialize(const base::ListValue* args) {
  const size_t kSimStatusInitializeParamCount = 1;
  double mode;
  if (args->GetSize() != kSimStatusInitializeParamCount ||
      !args->GetDouble(0, &mode)) {
    NOTREACHED();
    return;
  }
  dialog_mode_ = static_cast<SimDialogDelegate::SimDialogMode>(mode);
  VLOG(1) << "Initializing SIM dialog in mode: " << dialog_mode_;
  scoped_refptr<TaskProxy> task = new TaskProxy(AsWeakPtr());
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&TaskProxy::HandleInitialize, task.get()));
}

void SimUnlockHandler::InitializeSimStatus() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(armansito): For now, we're initializing the device path to the first
  // available cellular device. We should try to obtain a specific device here,
  // as there can be multiple cellular devices present.
  const DeviceState* cellular_device =
      GetNetworkStateHandler()
          ->GetDeviceStateByType(NetworkTypePattern::Cellular());
  if (cellular_device) {
    cellular_device_path_ = cellular_device->path();
    sim_lock_type_ = cellular_device->sim_lock_type();
  }
  ProcessSimCardState(cellular_device);
}

void SimUnlockHandler::ProceedToPukInput() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ProcessSimCardState(GetCellularDevice());
}

void SimUnlockHandler::ProcessSimCardState(
    const DeviceState* cellular) {
  std::string error_msg;
  if (cellular) {
    uint32 retries_left = cellular->sim_retries_left();
    VLOG(1) << "Current state: " << state_ << " lock_type: " << sim_lock_type_
            << " retries: " << retries_left;
    switch (state_) {
      case SIM_UNLOCK_LOADING:
        if (sim_lock_type_ == shill::kSIMLockPin) {
          state_ = SIM_LOCKED_PIN;
        } else if (sim_lock_type_ == shill::kSIMLockPuk) {
          if (retries_left > 0)
            state_ = SIM_LOCKED_PUK;
          else
            state_ = SIM_DISABLED;
        } else if (sim_lock_type_.empty()) {
          if (dialog_mode_ == SimDialogDelegate::SIM_DIALOG_SET_LOCK_ON ||
              dialog_mode_ == SimDialogDelegate::SIM_DIALOG_SET_LOCK_OFF) {
            state_ = SIM_NOT_LOCKED_ASK_PIN;
          } else if (dialog_mode_ == SimDialogDelegate::SIM_DIALOG_CHANGE_PIN) {
            state_ = SIM_NOT_LOCKED_CHANGE_PIN;
          } else {
            state_ = SIM_ABSENT_NOT_LOCKED;
          }
        } else {
          // SIM_UNKNOWN: when SIM status is not initialized (should not happen,
          // since this UI is accessible when SIM is initialized)
          // or SIM card is absent. In latter case just close dialog.
          state_ = SIM_ABSENT_NOT_LOCKED;
        }
        break;
      case SIM_ABSENT_NOT_LOCKED:
        // Dialog will close itself in this case.
        break;
      case SIM_NOT_LOCKED_ASK_PIN:
      case SIM_NOT_LOCKED_CHANGE_PIN:
        // We always start in these states when SIM is unlocked.
        // So if we get here while still being UNLOCKED,
        // that means entered PIN was incorrect.
        if (sim_lock_type_.empty()) {
          error_msg = kErrorPin;
        } else if (sim_lock_type_ == shill::kSIMLockPuk) {
          state_ = SIM_LOCKED_NO_PIN_TRIES_LEFT;
        } else {
          NOTREACHED()
              << "Change PIN / Set lock mode with unexpected SIM lock state";
          state_ = SIM_ABSENT_NOT_LOCKED;
        }
        break;
      case SIM_LOCKED_PIN:
        if (sim_lock_type_ == shill::kSIMLockPuk) {
          state_ = SIM_LOCKED_NO_PIN_TRIES_LEFT;
        } else if (sim_lock_type_ == shill::kSIMLockPin) {
          // Still locked with PIN.
          error_msg = kErrorPin;
        } else {
          state_ = SIM_ABSENT_NOT_LOCKED;
        }
        break;
      case SIM_LOCKED_NO_PIN_TRIES_LEFT:
        // Proceed user to PUK input.
        state_ = SIM_LOCKED_PUK;
        break;
      case SIM_LOCKED_PUK:
        if (sim_lock_type_ != shill::kSIMLockPin &&
            sim_lock_type_ != shill::kSIMLockPuk) {
          state_ = SIM_ABSENT_NOT_LOCKED;
        } else if (retries_left == 0) {
          state_ = SIM_LOCKED_NO_PUK_TRIES_LEFT;
        }
        // Otherwise SIM card is still locked with PUK code.
        // Dialog will display enter PUK screen with an updated retries count.
        break;
      case SIM_LOCKED_NO_PUK_TRIES_LEFT:
      case SIM_DISABLED:
        // User will close dialog manually.
        break;
    }
  } else {
    VLOG(1) << "Cellular device is absent.";
    // No cellular device, should close dialog.
    state_ = SIM_ABSENT_NOT_LOCKED;
  }
  VLOG(1) << "New state: " << state_;
  UpdatePage(cellular, error_msg);
}

void SimUnlockHandler::UpdatePage(const DeviceState* cellular,
                                  const std::string& error_msg) {
  base::DictionaryValue sim_dict;
  if (cellular)
    sim_dict.SetInteger(kTriesLeft, cellular->sim_retries_left());
  sim_dict.SetInteger(kState, state_);
  if (!error_msg.empty())
    sim_dict.SetString(kError, error_msg);
  else
    sim_dict.SetString(kError, kErrorOk);
  web_ui()->CallJavascriptFunction(kJsApiSimStatusChanged, sim_dict);
}

// SimUnlockUI -----------------------------------------------------------------

SimUnlockUI::SimUnlockUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  SimUnlockHandler* handler = new SimUnlockHandler();
  web_ui->AddMessageHandler(handler);
  SimUnlockUIHTMLSource* html_source = new SimUnlockUIHTMLSource();

  // Set up the chrome://sim-unlock/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(profile, html_source);
}

}  // namespace chromeos
