// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/inspect/inspect_ui.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_list.h"
#import "ios/chrome/browser/tabs/tab_model_list_observer.h"
#include "ios/chrome/browser/web/java_script_console/java_script_console_message.h"
#include "ios/chrome/browser/web/java_script_console/java_script_console_tab_helper.h"
#include "ios/chrome/browser/web/java_script_console/java_script_console_tab_helper_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"
#include "ios/chrome/grit/ios_resources.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/web_state/web_state.h"
#include "ios/web/public/web_ui_ios_data_source.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

web::WebUIIOSDataSource* CreateInspectUIHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUIInspectHost);

  source->AddLocalizedString("inspectConsoleNotice",
                             IDS_IOS_INSPECT_UI_CONSOLE_NOTICE);
  source->AddLocalizedString("inspectConsoleStartLogging",
                             IDS_IOS_INSPECT_UI_CONSOLE_START_LOGGING);
  source->AddLocalizedString("inspectConsoleStopLogging",
                             IDS_IOS_INSPECT_UI_CONSOLE_STOP_LOGGING);
  source->SetJsonPath("strings.js");
  source->AddResourcePath("inspect.js", IDR_IOS_INSPECT_JS);
  source->SetDefaultResource(IDR_IOS_INSPECT_HTML);
  source->UseGzip();
  return source;
}

// The handler for Javascript messages for the chrome://inspect/ page.
class InspectDOMHandler : public web::WebUIIOSMessageHandler,
                          public JavaScriptConsoleTabHelperDelegate,
                          public TabModelListObserver,
                          public WebStateListObserver {
 public:
  InspectDOMHandler();
  ~InspectDOMHandler() override;

  // WebUIIOSMessageHandler implementation
  void RegisterMessages() override;

  // JavaScriptConsoleTabHelperDelegate
  void DidReceiveConsoleMessage(
      const JavaScriptConsoleMessage& message) override;

  // TabModelListObserver
  void TabModelRegisteredWithBrowserState(
      TabModel* tab_model,
      ios::ChromeBrowserState* browser_state) override;
  void TabModelUnregisteredFromBrowserState(
      TabModel* tab_model,
      ios::ChromeBrowserState* browser_state) override;

  // WebStateListObserver
  void WebStateInsertedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index,
                          bool activating) override;

 private:
  // Handles the message from JavaScript to enable or disable console logging.
  void HandleSetLoggingEnabled(const base::ListValue* args);

  // Enables or disables console logging.
  void SetLoggingEnabled(bool enabled);

  // Sets |delegate| for the JavaScriptConsoleTabHelper associated with each web
  // state in |tab_model|.
  void SetDelegateForWebStatesInTabModel(
      TabModel* tab_model,
      JavaScriptConsoleTabHelperDelegate* delegate);

  // Whether or not logging is enabled.
  bool logging_enabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(InspectDOMHandler);
};

InspectDOMHandler::InspectDOMHandler() {}

InspectDOMHandler::~InspectDOMHandler() {
  // Clear delegate from WebStates.
  SetLoggingEnabled(false);
}

void InspectDOMHandler::HandleSetLoggingEnabled(const base::ListValue* args) {
  DCHECK_EQ(1u, args->GetSize());

  bool enabled = false;
  if (!args->GetBoolean(0, &enabled)) {
    NOTREACHED();
  }

  SetLoggingEnabled(enabled);
}

void InspectDOMHandler::SetLoggingEnabled(bool enabled) {
  if (logging_enabled_ == enabled) {
    return;
  }

  logging_enabled_ = enabled;

  web::BrowserState* browser_state = web_ui()->GetWebState()->GetBrowserState();
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(browser_state);
  NSArray<TabModel*>* tab_models =
      TabModelList::GetTabModelsForChromeBrowserState(chrome_browser_state);

  for (TabModel* tab_model in tab_models) {
    if (enabled) {
      tab_model.webStateList->AddObserver(this);
      SetDelegateForWebStatesInTabModel(tab_model, this);
    } else {
      tab_model.webStateList->RemoveObserver(this);
      SetDelegateForWebStatesInTabModel(tab_model, nullptr);
    }
  }

  if (enabled) {
    TabModelList::AddObserver(this);
  } else {
    TabModelList::RemoveObserver(this);
  }
}

void InspectDOMHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "setLoggingEnabled",
      base::BindRepeating(&InspectDOMHandler::HandleSetLoggingEnabled,
                          base::Unretained(this)));
}

void InspectDOMHandler::DidReceiveConsoleMessage(
    const JavaScriptConsoleMessage& message) {
  DVLOG(0) << message.origin.spec() << " [" << message.level << "] "
           << message.message->GetString();
}

void InspectDOMHandler::SetDelegateForWebStatesInTabModel(
    TabModel* tab_model,
    JavaScriptConsoleTabHelperDelegate* delegate) {
  for (int index = 0; index < tab_model.webStateList->count(); ++index) {
    web::WebState* web_state = tab_model.webStateList->GetWebStateAt(index);
    JavaScriptConsoleTabHelper::FromWebState(web_state)->SetDelegate(delegate);
  }
}

// TabModelListObserver
void InspectDOMHandler::TabModelRegisteredWithBrowserState(
    TabModel* tab_model,
    ios::ChromeBrowserState* browser_state) {
  tab_model.webStateList->AddObserver(this);
  SetDelegateForWebStatesInTabModel(tab_model, this);
}

void InspectDOMHandler::TabModelUnregisteredFromBrowserState(
    TabModel* tab_model,
    ios::ChromeBrowserState* browser_state) {
  tab_model.webStateList->RemoveObserver(this);
  SetDelegateForWebStatesInTabModel(tab_model, nullptr);
}

// WebStateListObserver
void InspectDOMHandler::WebStateInsertedAt(WebStateList* web_state_list,
                                           web::WebState* web_state,
                                           int index,
                                           bool activating) {
  JavaScriptConsoleTabHelper::FromWebState(web_state)->SetDelegate(this);
}

}  // namespace

InspectUI::InspectUI(web::WebUIIOS* web_ui) : web::WebUIIOSController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<InspectDOMHandler>());

  web::WebUIIOSDataSource::Add(ios::ChromeBrowserState::FromWebUIIOS(web_ui),
                               CreateInspectUIHTMLSource());
}

InspectUI::~InspectUI() {}
