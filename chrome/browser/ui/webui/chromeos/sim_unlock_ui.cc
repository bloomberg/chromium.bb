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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// JS API callbacks names.
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

chromeos::CellularNetwork* GetCellularNetwork() {
  chromeos::NetworkLibrary* lib = chromeos::CrosLibrary::Get()->
      GetNetworkLibrary();
  if (lib &&
      lib->cellular_networks().begin() != lib->cellular_networks().end()) {
    return *(lib->cellular_networks().begin());
  }
  return NULL;
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
                         public base::SupportsWeakPtr<SimUnlockHandler> {
 public:
  SimUnlockHandler();
  virtual ~SimUnlockHandler();

  // Init work after Attach.
  void Init(TabContents* contents);

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui);
  virtual void RegisterMessages();

 private:
  // Should keep this state enum in sync with similar one in JS code.
  typedef enum SimUnlockState {
    SIM_UNLOCK_LOADING           = -1,
    SIM_ABSEND_NOT_LOCKED        =  0,
    SIM_LOCKED_PIN               =  1,
    SIM_LOCKED_NO_PIN_TRIES_LEFT =  2,
    SIM_LOCKED_PUK               =  3,
    SIM_LOCKED_NO_PUK_TRIES_LEFT =  4,
    SIM_DISABLED                 =  5,
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

  // Pass PIN/PUK code to flimflam and check status.
  void EnterCode(const std::string& code, SimUnlockCode code_type);

  // Exports SIM card state.
  void GetSimInfo(chromeos::CellularNetwork* network, DictionaryValue* value);

  // Single handler for PIN/PUK code input JS callbacks.
  void HandleEnterCode(const ListValue* args, SimUnlockCode code_type);

  // Handlers for JS WebUI messages.
  void HandleEnterPinCode(const ListValue* args);
  void HandleEnterPukCode(const ListValue* args);
  void HandleProceedToPukInput(const ListValue* args);
  void HandleSimStatusInitialize(const ListValue* args);

  // Initialize current SIM card status, passes that to page.
  void InitializeSimStatus();

  // Checks whether SIM card is in PUK locked state and proceeds to PUK input.
  void ProceedToPukInput();

  // Processes current SIM card state and update internal state/page.
  void ProcessSimCardState(chromeos::CellularNetwork* network);

  // Updates page with the current state/SIM card info/error.
  void UpdatePage(chromeos::CellularNetwork* network,
                  const std::string& error_msg);

  TabContents* tab_contents_;
  SimUnlockState state_;

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
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_ENTER_PIN_MESSAGE));
  strings.SetString("enterPinTriesMessage",
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_INCORRECT_PIN_TRIES_MESSAGE));
  strings.SetString("incorrectPinMessage",
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_INCORRECT_PIN_MESSAGE));
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
  strings.SetString("noPukTriesLeft",
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_NO_PUK_TRIES_LEFT_MESSAGE));
  strings.SetString("simDisabledTitle",
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_SIM_DISABLED_TITLE));
  strings.SetString("simDisabledMessage",
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_SIM_DISABLED_MESSAGE));
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
      state_(SIM_UNLOCK_LOADING) {
}

SimUnlockHandler::~SimUnlockHandler() {
}

WebUIMessageHandler* SimUnlockHandler::Attach(WebUI* web_ui) {
  return WebUIMessageHandler::Attach(web_ui);
}

void SimUnlockHandler::Init(TabContents* contents) {
  tab_contents_ = contents;
  // TODO(nkostylev): Init SIM lock/unlock state.
}

void SimUnlockHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback(kJsApiEnterPinCode,
      NewCallback(this, &SimUnlockHandler::HandleEnterPinCode));
  web_ui_->RegisterMessageCallback(kJsApiEnterPukCode,
      NewCallback(this, &SimUnlockHandler::HandleEnterPukCode));
  web_ui_->RegisterMessageCallback(kJsApiProceedToPukInput,
      NewCallback(this, &SimUnlockHandler::HandleProceedToPukInput));
  web_ui_->RegisterMessageCallback(kJsApiSimStatusInitialize,
      NewCallback(this, &SimUnlockHandler::HandleSimStatusInitialize));
}

void SimUnlockHandler::EnterCode(const std::string& code,
                                 SimUnlockCode code_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // TODO(nkostylev): Pass PIN/PUK code to flimflam.
  ProcessSimCardState(GetCellularNetwork());
}

void SimUnlockHandler::GetSimInfo(chromeos::CellularNetwork* network,
                                  DictionaryValue* value) {
  if (network) {
    // TODO(nkostylev): Extract real tries left information.
    // value->SetInteger(kTriesLeft, tries);
  }
}

void SimUnlockHandler::HandleEnterCode(const ListValue* args,
                                       SimUnlockCode code_type) {
  const size_t kEnterCodeParamCount = 1;
  std::string code;
  if (args->GetSize() != kEnterCodeParamCount ||
      !args->GetString(0, &code)) {
    NOTREACHED();
    return;
  }
  scoped_refptr<TaskProxy> task = new TaskProxy(AsWeakPtr(), code, code_type);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(task.get(), &TaskProxy::HandleEnterCode));
}

void SimUnlockHandler::HandleEnterPinCode(const ListValue* args) {
  HandleEnterCode(args, CODE_PIN);
}

void SimUnlockHandler::HandleEnterPukCode(const ListValue* args) {
  HandleEnterCode(args, CODE_PUK);
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
  scoped_refptr<TaskProxy> task = new TaskProxy(AsWeakPtr());
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(task.get(), &TaskProxy::HandleInitialize));
}

void SimUnlockHandler::InitializeSimStatus() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ProcessSimCardState(GetCellularNetwork());
}

void SimUnlockHandler::ProceedToPukInput() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ProcessSimCardState(GetCellularNetwork());
}

void SimUnlockHandler::ProcessSimCardState(chromeos::CellularNetwork* network) {
  std::string error_msg;
  // TODO(nkostylev): Get real SIM card status:
  // Absent/Not Locked/PIN locked, n tries/PUK locked, n tries/Blocked.
  switch (state_) {
    case SIM_UNLOCK_LOADING:
    case SIM_ABSEND_NOT_LOCKED:
    case SIM_LOCKED_PIN:
      break;
    case SIM_LOCKED_NO_PIN_TRIES_LEFT:
      state_ = SIM_LOCKED_PUK;
      break;
    case SIM_LOCKED_PUK:
    case SIM_LOCKED_NO_PUK_TRIES_LEFT:
    case SIM_DISABLED:
      break;
  }
  UpdatePage(network, error_msg);
}

void SimUnlockHandler::UpdatePage(chromeos::CellularNetwork* network,
                                  const std::string& error_msg) {
  DictionaryValue sim_dict;
  if (network)
    GetSimInfo(network, &sim_dict);
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
