// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"

#include <string>

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/string_piece.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// JS API callbacks names.
const char kJsApiScreenStateInitialize[] = "screenStateInitialize";

// Page JS API function names.
// const char kJsApiScreenStateChanged[] = "cr.ui.Oobe.screenStateChanged";

// OOBE screen state variables which are passed to the page.
const char kState[] = "state";

}  // namespace

namespace chromeos {

class OobeUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  OobeUIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  virtual ~OobeUIHTMLSource() {}

  std::string service_path_;
  DISALLOW_COPY_AND_ASSIGN(OobeUIHTMLSource);
};

// The handler for Javascript messages related to the "oobe" view.
class OobeHandler : public WebUIMessageHandler,
                    public base::SupportsWeakPtr<OobeHandler> {
 public:
  OobeHandler();
  virtual ~OobeHandler();

  // Init work after Attach.
  void Init(TabContents* contents);

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui);
  virtual void RegisterMessages();

 private:
  // Should keep this state enum in sync with similar one in JS code.
  typedef enum ScreenState {
    SCREEN_LOADING = -1,
    SCREEN_NONE    =  0,
    SCREEN_WELCOME =  1,
    SCREEN_EULA    =  2,
    SCREEN_UPDATE  =  3,
  } ScreenState;

  class TaskProxy : public base::RefCountedThreadSafe<TaskProxy> {
   public:
    explicit TaskProxy(const base::WeakPtr<OobeHandler>& handler)
        : handler_(handler) {
    }

    void HandleInitialize() {
      if (handler_)
        handler_->InitializeScreenState();
    }

   private:
    base::WeakPtr<OobeHandler> handler_;

    DISALLOW_COPY_AND_ASSIGN(TaskProxy);
  };

  // Handlers for JS WebUI messages.
  void HandleScreenStateInitialize(const ListValue* args);

  // Initializes current OOBE state, passes that to page.
  void InitializeScreenState();

  // Updates page states.
  void UpdatePage();

  TabContents* tab_contents_;
  ScreenState state_;

  DISALLOW_COPY_AND_ASSIGN(OobeHandler);
};

// OobeUIHTMLSource -------------------------------------------------------

OobeUIHTMLSource::OobeUIHTMLSource()
    : DataSource(chrome::kChromeUIOobeHost, MessageLoop::current()) {
}

void OobeUIHTMLSource::StartDataRequest(const std::string& path,
                                        bool is_incognito,
                                        int request_id) {
  DictionaryValue strings;
  // OOBE title is not actually seen in UI, use title of the welcome screen.
  strings.SetString("title",
                    l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_TITLE));
  strings.SetString("welcomeScreenTitle",
                    l10n_util::GetStringFUTF16(IDS_WELCOME_SCREEN_TITLE,
                        l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME)));
  strings.SetString("languageSelect",
                    l10n_util::GetStringUTF16(IDS_LANGUAGE_SELECTION_SELECT));
  strings.SetString("keyboardSelect",
                    l10n_util::GetStringUTF16(IDS_KEYBOARD_SELECTION_SELECT));
  strings.SetString("networkSelect",
                    l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_SELECT));
  strings.SetString("continue",
      l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_CONTINUE_BUTTON));
  strings.SetString("eulaScreenTitle",
                    l10n_util::GetStringFUTF16(IDS_EULA_SCREEN_TITLE,
                        l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME)));
  strings.SetString("checkboxLogging",
      l10n_util::GetStringUTF16(IDS_EULA_CHECKBOX_ENABLE_LOGGING));
  strings.SetString("learnMore",
                    l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  strings.SetString("back",
                    l10n_util::GetStringUTF16(IDS_EULA_BACK_BUTTON));
  strings.SetString("acceptAgreement",
      l10n_util::GetStringUTF16(IDS_EULA_ACCEPT_AND_CONTINUE_BUTTON));
  SetFontAndTextDirection(&strings);

  static const base::StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(IDR_OOBE_HTML));

  const std::string& full_html = jstemplate_builder::GetI18nTemplateHtml(
      html, &strings);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes());
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

// OobeHandler ------------------------------------------------------------

OobeHandler::OobeHandler()
    : tab_contents_(NULL),
      state_(SCREEN_LOADING) {
}

OobeHandler::~OobeHandler() {
}

WebUIMessageHandler* OobeHandler::Attach(WebUI* web_ui) {
  return WebUIMessageHandler::Attach(web_ui);
}

void OobeHandler::Init(TabContents* contents) {
  tab_contents_ = contents;
}

void OobeHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback(kJsApiScreenStateInitialize,
      NewCallback(this, &OobeHandler::HandleScreenStateInitialize));
}

void OobeHandler::HandleScreenStateInitialize(const ListValue* args) {
  const size_t kScreenStateInitializeParamCount = 0;
  if (args->GetSize() != kScreenStateInitializeParamCount) {
    NOTREACHED();
    return;
  }
  scoped_refptr<TaskProxy> task = new TaskProxy(AsWeakPtr());
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(task.get(), &TaskProxy::HandleInitialize));
}

void OobeHandler::InitializeScreenState() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // TODO(nkostylev): Integrated with OOBE flow, controllers.
  state_ = SCREEN_WELCOME;
  UpdatePage();
}

void OobeHandler::UpdatePage() {
  DictionaryValue screen_info_dict;
  VLOG(1) << "New state: " << state_;
  screen_info_dict.SetInteger(kState, state_);
  // TODO(nkostylev): Initialize page state.
  // web_ui_->CallJavascriptFunction(kJsApiScreenStateChanged,
  //                                 screen_info_dict);
}

// OobeUI ----------------------------------------------------------------------

OobeUI::OobeUI(TabContents* contents) : WebUI(contents) {
  OobeHandler* handler = new OobeHandler();
  AddMessageHandler((handler)->Attach(this));
  handler->Init(contents);
  OobeUIHTMLSource* html_source = new OobeUIHTMLSource();

  // Set up the chrome://oobe/ source.
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);
}

}  // namespace chromeos
