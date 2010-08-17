// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/remoting/remoting_setup_flow.h"

#include "app/gfx/font_util.h"
#include "base/json/json_writer.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/dom_ui/dom_ui_util.h"
#include "chrome/browser/google_service_auth_error.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/remoting/remoting_resources_source.h"
#include "chrome/browser/remoting/remoting_setup_message_handler.h"
#include "chrome/common/pref_names.h"
#include "gfx/font.h"
#include "grit/locale_settings.h"

// Use static Run method to get an instance.
RemotingSetupFlow::RemotingSetupFlow(const std::string& args, Profile* profile)
    : message_handler_(NULL),
      dialog_start_args_(args),
      profile_(profile) {
  // TODO(hclam): We are currently leaking this objcet. Need to fix this!
  message_handler_ = new RemotingSetupMessageHandler(this);
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(Singleton<ChromeURLDataManager>::get(),
                        &ChromeURLDataManager::AddDataSource,
                        make_scoped_refptr(new RemotingResourcesSource())));
}

RemotingSetupFlow::~RemotingSetupFlow() {
}

void RemotingSetupFlow::GetDialogSize(gfx::Size* size) const {
  PrefService* prefs = profile_->GetPrefs();
  gfx::Font approximate_web_font(
      UTF8ToWide(prefs->GetString(prefs::kWebKitSansSerifFontFamily)),
      prefs->GetInteger(prefs::kWebKitDefaultFontSize));

  // TODO(pranavk) Replace the following SYNC resources with REMOTING Resources.
  *size = gfx::GetLocalizedContentsSizeForFont(
      IDS_SYNC_SETUP_WIZARD_WIDTH_CHARS,
      IDS_SYNC_SETUP_WIZARD_HEIGHT_LINES,
      approximate_web_font);
}

// A callback to notify the delegate that the dialog closed.
void RemotingSetupFlow::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

// static
void RemotingSetupFlow::GetArgsForGaiaLogin(DictionaryValue* args) {
  args->SetString(L"iframeToShow", "login");
  args->SetString(L"user", std::wstring());
  args->SetInteger(L"error", 0);
  args->SetBoolean(L"editable_user", true);
}

void RemotingSetupFlow::GetDOMMessageHandlers(
    std::vector<DOMMessageHandler*>* handlers) const {
  // Create the message handler only after we are asked.
  handlers->push_back(message_handler_);
}

void RemotingSetupFlow::Focus() {
  // TODO(pranavk): implement this method.
  NOTIMPLEMENTED();
}

void RemotingSetupFlow::OnUserSubmittedAuth(const std::string& user,
                                            const std::string& password,
                                            const std::string& captcha) {
  // TODO(hclam): Should do the following two things.
  // 1. Authenicate using the info.
  // 2. Register the host service.
  message_handler_->ShowGaiaSuccessAndSettingUp();

  // TODO(hclam): This is very unsafe because |message_handler_| is not
  // refcounted. Destruction of the handler before the timeout will cause
  // memory error.
  // This services as a demo to show that we can do authenication asynchronously
  // and setting up stuff and then present the done page later.
  ChromeThread::PostDelayedTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(message_handler_,
                        &RemotingSetupMessageHandler::ShowSetupDone),
      2000);
}

// static
RemotingSetupFlow* RemotingSetupFlow::Run(Profile* profile) {
  // Set the arguments for showing the gaia login page.
  DictionaryValue args;
  GetArgsForGaiaLogin(&args);
  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  RemotingSetupFlow* flow = new RemotingSetupFlow(json_args, profile);
  Browser* b = BrowserList::GetLastActive();
  if (b) {
    b->BrowserShowHtmlDialog(flow, NULL);
  } else {
    delete flow;
    return NULL;
  }
  return flow;
}

// Global function to start the remoting setup dialog.
void OpenRemotingSetupDialog(Profile* profile) {
  RemotingSetupFlow::Run(profile->GetOriginalProfile());
}

DISABLE_RUNNABLE_METHOD_REFCOUNT(RemotingSetupMessageHandler);
