// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/sim_unlock_ui.h"

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/string_piece.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Host page JS API function names.
const char kJsApiEnterPinCode[] = "enterPinCode";
const char kJsApiEnterPukCode[] = "enterPukCode";

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
  // Handlers for JS WebUI messages.
  void HandleEnterPinCode(const ListValue* args);
  void HandleEnterPukCode(const ListValue* args);

  TabContents* tab_contents_;

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
  strings.SetString("enterPinTitle",
                    l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_ENTER_PIN_TITLE));
  strings.SetString("incorrectPinMessage",
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_INCORRECT_PIN_MESSAGE));
  // TODO(nkostylev): Initialize string with number of PIN tries left.
  strings.SetString("pinTriesLeft",
      l10n_util::GetStringFUTF16(IDS_SIM_UNLOCK_PIN_TRIES_LEFT_MESSAGE,
                                 string16()));
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
  // TODO(nkostylev): Initialize string with number of PIN tries left.
  strings.SetString("pukTriesLeft",
      l10n_util::GetStringFUTF16(IDS_SIM_UNLOCK_PUK_TRIES_LEFT_MESSAGE,
                                 string16()));
  // TODO(nkostylev): Pass carrier name if we know that.
  strings.SetString("enterPukMessage", l10n_util::GetStringFUTF16(
      IDS_SIM_UNLOCK_ENTER_PUK_MESSAGE,
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_DEFAULT_CARRIER)));
  strings.SetString("noPukTriesLeft",
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_NO_PUK_TRIES_LEFT_MESSAGE));
  strings.SetString("simBlockedTitle",
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_SIM_BLOCKED_TITLE));
  strings.SetString("simBlockedMessage",
      l10n_util::GetStringUTF16(IDS_SIM_UNLOCK_SIM_BLOCKED_MESSAGE));
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
    : tab_contents_(NULL) {
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
}

void SimUnlockHandler::HandleEnterPinCode(const ListValue* args) {
  // TODO(nkostylev): Pass PIN code to flimflam.
}

void SimUnlockHandler::HandleEnterPukCode(const ListValue* args) {
  // TODO(nkostylev): Pass PUK code to flimflam.
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
