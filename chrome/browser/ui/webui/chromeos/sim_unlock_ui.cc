// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/sim_unlock_ui.h"

#include <string>

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/string_piece.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/sim_dialog_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// JS API callbacks names.
const char kJsApiCancel[] = "cancel";
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
const char kErrorPuk[] = "incorrectPuk";
const char kErrorOk[] = "ok";

const chromeos::NetworkDevice* GetCellularDevice() {
  chromeos::NetworkLibrary* lib = chromeos::CrosLibrary::Get()->
      GetNetworkLibrary();
  CHECK(lib);
  return lib->FindCellularDevice();
}

}  // namespace

namespace chromeos {

class SimUnlockUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  SimUnlockUIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  virtual ~SimUnlockUIHTMLSource() {}

  std::string service_path_;
  DISALLOW_COPY_AND_ASSIGN(SimUnlockUIHTMLSource);
};

// The handler for Javascript messages related to the "sim-unlock" view.
class SimUnlockHandler : public WebUIMessageHandler,
                         public base::SupportsWeakPtr<SimUnlockHandler>,
                         public NetworkLibrary::NetworkDeviceObserver,
                         public NetworkLibrary::PinOperationObserver {
 public:
  SimUnlockHandler();
  virtual ~SimUnlockHandler();

  // Init work after Attach.
  void Init(TabContents* contents);

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui);
  virtual void RegisterMessages();

  // NetworkLibrary::NetworkDeviceObserver implementation.
  virtual void OnNetworkDeviceChanged(NetworkLibrary* cros,
                                      const NetworkDevice* device);

  // NetworkLibrary::PinOperationObserver implementation.
  virtual void OnPinOperationCompleted(NetworkLibrary* cros,
                                       PinOperationError error);

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
  typedef enum SimUnlockCode {
    CODE_PIN,
    CODE_PUK,
  } SimUnlockCode;

  class TaskProxy : public base::RefCountedThreadSafe<TaskProxy> {
   public:
    explicit TaskProxy(const base::WeakPtr<SimUnlockHandler>& handler)
        : handler_(handler) {
    }

    TaskProxy(const base::WeakPtr<SimUnlockHandler>& handler,
              const std::string& code,
              SimUnlockCode code_type)
        : handler_(handler),
          code_(code),
          code_type_(code_type) {
    }

    void HandleCancel() {
      if (handler_)
        handler_->CancelDialog();
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
    base::WeakPtr<SimUnlockHandler> handler_;

    // Pending code input (PIN/PUK).
    std::string code_;

    // Pending code type.
    SimUnlockCode code_type_;

    DISALLOW_COPY_AND_ASSIGN(TaskProxy);
  };

  // Processing for the cases when dialog was cancelled.
  void CancelDialog();

  // Pass PIN/PUK code to flimflam and check status.
  void EnterCode(const std::string& code, SimUnlockCode code_type);

  // Single handler for PIN/PUK code operations.
  void HandleEnterCode(SimUnlockCode code_type, const std::string& code);

  // Handlers for JS WebUI messages.
  void HandleCancel(const ListValue* args);
  void HandleChangePinCode(const ListValue* args);
  void HandleEnterPinCode(const ListValue* args);
  void HandleEnterPukCode(const ListValue* args);
  void HandleProceedToPukInput(const ListValue* args);
  void HandleSimStatusInitialize(const ListValue* args);

  // Initialize current SIM card status, passes that to page.
  void InitializeSimStatus();

  // Notifies SIM Security tab handler that RequirePin preference change
  // has been ended (either updated or cancelled).
  void NotifyOnRequirePinChangeEnded(bool new_value);

  // Notifies observers that the EnterPin or EnterPuk dialog has been
  // completed (either cancelled or with entry of PIN/PUK).
  void NotifyOnEnterPinEnded(bool cancelled);

  // Checks whether SIM card is in PUK locked state and proceeds to PUK input.
  void ProceedToPukInput();

  // Processes current SIM card state and update internal state/page.
  void ProcessSimCardState(const chromeos::NetworkDevice* cellular);

  // Updates page with the current state/SIM card info/error.
  void UpdatePage(const chromeos::NetworkDevice* cellular,
                  const std::string& error_msg);

  TabContents* tab_contents_;
  SimUnlockState state_;

  // Path of the Cellular device that we monitor property updates from.
  std::string cellular_device_path_;

  // Type of the dialog: generic unlock/change pin/change PinRequire.
  SimDialogDelegate::SimDialogMode dialog_mode_;

  // New PIN value for the case when we unblock SIM card or change PIN.
  std::string new_pin_;

  DISALLOW_COPY_AND_ASSIGN(SimUnlockHandler);
};

// SimUnlockUIHTMLSource -------------------------------------------------------

SimUnlockUIHTMLSource::SimUnlockUIHTMLSource()
    : DataSource(chrome::kChromeUISimUnlockHost, MessageLoop::current()) {
}

void SimUnlockUIHTMLSource::StartDataRequest(const std::string& path,
                                             bool is_incognito,
                                             int request_id) {
  DictionaryValue strings;
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

  SetFontAndTextDirection(&strings);

  static const base::StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_SIM_UNLOCK_HTML));

  const std::string& full_html = jstemplate_builder::GetI18nTemplateHtml(
      html, &strings);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes());
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

// SimUnlockHandler ------------------------------------------------------------

SimUnlockHandler::SimUnlockHandler()
    : tab_contents_(NULL),
      state_(SIM_UNLOCK_LOADING),
      dialog_mode_(SimDialogDelegate::SIM_DIALOG_UNLOCK) {
  const chromeos::NetworkDevice* cellular = GetCellularDevice();
  // One could just call us directly via chrome://sim-unlock.
  if (cellular) {
    cellular_device_path_ = cellular->device_path();
    CrosLibrary::Get()->GetNetworkLibrary()->AddNetworkDeviceObserver(
        cellular_device_path_, this);
    CrosLibrary::Get()->GetNetworkLibrary()->AddPinOperationObserver(this);
  }
}

SimUnlockHandler::~SimUnlockHandler() {
  if (!cellular_device_path_.empty()) {
    CrosLibrary::Get()->GetNetworkLibrary()->RemoveNetworkDeviceObserver(
        cellular_device_path_, this);
    CrosLibrary::Get()->GetNetworkLibrary()->RemovePinOperationObserver(this);
  }
}

WebUIMessageHandler* SimUnlockHandler::Attach(WebUI* web_ui) {
  return WebUIMessageHandler::Attach(web_ui);
}

void SimUnlockHandler::Init(TabContents* contents) {
  tab_contents_ = contents;
}

void SimUnlockHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback(kJsApiCancel,
        NewCallback(this, &SimUnlockHandler::HandleCancel));
  web_ui_->RegisterMessageCallback(kJsApiChangePinCode,
      NewCallback(this, &SimUnlockHandler::HandleChangePinCode));
  web_ui_->RegisterMessageCallback(kJsApiEnterPinCode,
      NewCallback(this, &SimUnlockHandler::HandleEnterPinCode));
  web_ui_->RegisterMessageCallback(kJsApiEnterPukCode,
      NewCallback(this, &SimUnlockHandler::HandleEnterPukCode));
  web_ui_->RegisterMessageCallback(kJsApiProceedToPukInput,
      NewCallback(this, &SimUnlockHandler::HandleProceedToPukInput));
  web_ui_->RegisterMessageCallback(kJsApiSimStatusInitialize,
      NewCallback(this, &SimUnlockHandler::HandleSimStatusInitialize));
}

void SimUnlockHandler::OnNetworkDeviceChanged(NetworkLibrary* cros,
                                              const NetworkDevice* device) {
  chromeos::SIMLockState lock_state = device->sim_lock_state();
  int retries_left = device->sim_retries_left();
  VLOG(1) << "OnNetworkDeviceChanged, lock: " << lock_state
          << ", retries: " << retries_left;
  ProcessSimCardState(GetCellularDevice());
}

void SimUnlockHandler::OnPinOperationCompleted(NetworkLibrary* cros,
                                               PinOperationError error) {
  DCHECK(cros);
  const NetworkDevice* cellular = cros->FindCellularDevice();
  DCHECK(cellular);
  VLOG(1) << "OnPinOperationCompleted, error: " << error;
  if (state_ == SIM_NOT_LOCKED_ASK_PIN && error == PIN_ERROR_NONE) {
    CHECK(dialog_mode_ == SimDialogDelegate::SIM_DIALOG_SET_LOCK_ON ||
          dialog_mode_ == SimDialogDelegate::SIM_DIALOG_SET_LOCK_OFF);
    // Async change RequirePin operation has finished OK.
    NotifyOnRequirePinChangeEnded(
        dialog_mode_ == SimDialogDelegate::SIM_DIALOG_SET_LOCK_ON);
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
  if (dialog_mode_ == SimDialogDelegate::SIM_DIALOG_UNLOCK &&
      state_ == SIM_ABSENT_NOT_LOCKED)
    NotifyOnEnterPinEnded(false);
}

void SimUnlockHandler::CancelDialog() {
  if (dialog_mode_ == SimDialogDelegate::SIM_DIALOG_SET_LOCK_ON ||
      dialog_mode_ == SimDialogDelegate::SIM_DIALOG_SET_LOCK_OFF) {
    // When async change RequirePin operation is performed,
    // dialog UI controls such as Cancel button are disabled.
    // If dialog was cancelled that means RequirePin preference hasn't been
    // changed and is not in process of changing at the moment.
    NotifyOnRequirePinChangeEnded(
        !(dialog_mode_ == SimDialogDelegate::SIM_DIALOG_SET_LOCK_ON));
  } else if (dialog_mode_ == SimDialogDelegate::SIM_DIALOG_UNLOCK) {
    NotifyOnEnterPinEnded(true);
  }
}

void SimUnlockHandler::EnterCode(const std::string& code,
                                 SimUnlockCode code_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  NetworkLibrary* lib = chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  CHECK(lib);

  const NetworkDevice* cellular = GetCellularDevice();
  chromeos::SIMLockState lock_state = cellular->sim_lock_state();

  switch (code_type) {
    case CODE_PIN:
      if (dialog_mode_ == SimDialogDelegate::SIM_DIALOG_SET_LOCK_ON ||
          dialog_mode_ == SimDialogDelegate::SIM_DIALOG_SET_LOCK_OFF) {
        if (lock_state != chromeos::SIM_UNLOCKED) {
          // If SIM is locked/absent, change RequirePin UI is not accessible.
          NOTREACHED() <<
              "Changing RequirePin pref on locked / uninitialized SIM.";
        }
        lib->ChangeRequirePin(
            dialog_mode_ == SimDialogDelegate::SIM_DIALOG_SET_LOCK_ON,
            code);
      } else if (dialog_mode_ == SimDialogDelegate::SIM_DIALOG_CHANGE_PIN) {
        if (lock_state != chromeos::SIM_UNLOCKED) {
          // If SIM is locked/absent, changing PIN UI is not accessible.
          NOTREACHED() << "Changing PIN on locked / uninitialized SIM.";
        }
        lib->ChangePin(code, new_pin_);
      } else {
        lib->EnterPin(code);
      }
      break;
    case CODE_PUK:
      DCHECK(!new_pin_.empty());
      lib->UnblockPin(code, new_pin_);
      break;
  }
}

void SimUnlockHandler::NotifyOnEnterPinEnded(bool cancelled) {
  NotificationService::current()->Notify(
      NotificationType::ENTER_PIN_ENDED,
      NotificationService::AllSources(),
      Details<bool>(&cancelled));
}

void SimUnlockHandler::NotifyOnRequirePinChangeEnded(bool new_value) {
  NotificationService::current()->Notify(
      NotificationType::REQUIRE_PIN_SETTING_CHANGE_ENDED,
      NotificationService::AllSources(),
      Details<bool>(&new_value));
}

void SimUnlockHandler::HandleCancel(const ListValue* args) {
  const size_t kEnterCodeParamCount = 0;
  if (args->GetSize() != kEnterCodeParamCount) {
    NOTREACHED();
    return;
  }
  scoped_refptr<TaskProxy> task = new TaskProxy(AsWeakPtr());
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(task.get(), &TaskProxy::HandleCancel));
}

void SimUnlockHandler::HandleChangePinCode(const ListValue* args) {
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
      NewRunnableMethod(task.get(), &TaskProxy::HandleEnterCode));
}

void SimUnlockHandler::HandleEnterPinCode(const ListValue* args) {
  const size_t kEnterPinParamCount = 1;
  std::string pin;
  if (args->GetSize() != kEnterPinParamCount || !args->GetString(0, &pin)) {
    NOTREACHED();
    return;
  }
  HandleEnterCode(CODE_PIN, pin);
}

void SimUnlockHandler::HandleEnterPukCode(const ListValue* args) {
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

void SimUnlockHandler::HandleProceedToPukInput(const ListValue* args) {
  const size_t kProceedToPukInputParamCount = 0;
  if (args->GetSize() != kProceedToPukInputParamCount) {
    NOTREACHED();
    return;
  }
  scoped_refptr<TaskProxy> task = new TaskProxy(AsWeakPtr());
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(task.get(), &TaskProxy::HandleProceedToPukInput));
}

void SimUnlockHandler::HandleSimStatusInitialize(const ListValue* args) {
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
      NewRunnableMethod(task.get(), &TaskProxy::HandleInitialize));
}

void SimUnlockHandler::InitializeSimStatus() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ProcessSimCardState(GetCellularDevice());
}

void SimUnlockHandler::ProceedToPukInput() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ProcessSimCardState(GetCellularDevice());
}

void SimUnlockHandler::ProcessSimCardState(
    const chromeos::NetworkDevice* cellular) {
  std::string error_msg;
  if (cellular) {
    chromeos::SIMLockState lock_state = cellular->sim_lock_state();
    int retries_left = cellular->sim_retries_left();
    VLOG(1) << "Current state: " << state_ << " lock_state: " << lock_state
            << " retries: " << retries_left;
    switch (state_) {
      case SIM_UNLOCK_LOADING:
        if (lock_state == chromeos::SIM_LOCKED_PIN) {
          state_ = SIM_LOCKED_PIN;
        } else if (lock_state == chromeos::SIM_LOCKED_PUK) {
          if (retries_left > 0)
            state_ = SIM_LOCKED_PUK;
          else
            state_ = SIM_DISABLED;
        } else if (lock_state == chromeos::SIM_UNLOCKED) {
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
        if (lock_state == chromeos::SIM_UNLOCKED) {
          error_msg = kErrorPin;
        } else if (lock_state == chromeos::SIM_LOCKED_PUK) {
          state_ = SIM_LOCKED_NO_PIN_TRIES_LEFT;
        } else {
          NOTREACHED()
              << "Change PIN / Set lock mode with unexpected SIM lock state";
          state_ = SIM_ABSENT_NOT_LOCKED;
        }
        break;
      case SIM_LOCKED_PIN:
        if (lock_state == chromeos::SIM_UNLOCKED ||
            lock_state == chromeos::SIM_UNKNOWN) {
          state_ = SIM_ABSENT_NOT_LOCKED;
        } else if (lock_state == chromeos::SIM_LOCKED_PUK) {
          state_ = SIM_LOCKED_NO_PIN_TRIES_LEFT;
        } else {
          // Still blocked with PIN.
          error_msg = kErrorPin;
        }
        break;
      case SIM_LOCKED_NO_PIN_TRIES_LEFT:
        // Proceed user to PUK input.
        state_ = SIM_LOCKED_PUK;
        break;
      case SIM_LOCKED_PUK:
        if (lock_state == chromeos::SIM_UNLOCKED ||
            lock_state == chromeos::SIM_UNKNOWN) {
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

void SimUnlockHandler::UpdatePage(const chromeos::NetworkDevice* cellular,
                                  const std::string& error_msg) {
  DictionaryValue sim_dict;
  if (cellular)
    sim_dict.SetInteger(kTriesLeft, cellular->sim_retries_left());
  sim_dict.SetInteger(kState, state_);
  if (!error_msg.empty())
    sim_dict.SetString(kError, error_msg);
  else
    sim_dict.SetString(kError, kErrorOk);
  web_ui_->CallJavascriptFunction(kJsApiSimStatusChanged, sim_dict);
}

// SimUnlockUI -----------------------------------------------------------------

SimUnlockUI::SimUnlockUI(TabContents* contents) : WebUI(contents) {
  SimUnlockHandler* handler = new SimUnlockHandler();
  AddMessageHandler((handler)->Attach(this));
  handler->Init(contents);
  SimUnlockUIHTMLSource* html_source = new SimUnlockUIHTMLSource();

  // Set up the chrome://sim-unlock/ source.
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);
}

}  // namespace chromeos
