// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_setup_wizard.h"

#include "app/resource_bundle.h"
#include "base/message_loop.h"
#include "base/singleton.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_setup_flow.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "grit/app_resources.h"
#include "grit/browser_resources.h"

class SyncResourcesSource : public ChromeURLDataManager::DataSource {
 public:
  SyncResourcesSource()
      : DataSource(chrome::kSyncResourcesHost, MessageLoop::current()) {
  }

  virtual void StartDataRequest(const std::string& path, int request_id);

  virtual std::string GetMimeType(const std::string& path) const {
    if (path == chrome::kSyncThrobberPath)
      return "image/png";
    else
      return "text/html";
  }

 private:
  virtual ~SyncResourcesSource() {}

  DISALLOW_COPY_AND_ASSIGN(SyncResourcesSource);
};

void SyncResourcesSource::StartDataRequest(const std::string& path_raw,
                                           int request_id) {
  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  if (path_raw == chrome::kSyncThrobberPath) {
    scoped_refptr<RefCountedMemory> throbber(
        ResourceBundle::GetSharedInstance().LoadImageResourceBytes(
            IDR_THROBBER));
    SendResponse(request_id, throbber);
    return;
  }

  std::string response;
  if (path_raw == chrome::kSyncGaiaLoginPath) {
    DictionaryValue localized_strings;
    localized_strings.SetString(L"settingupsync",
        l10n_util::GetString(IDS_SYNC_LOGIN_SETTING_UP_SYNC));
    localized_strings.SetString(L"introduction",
        l10n_util::GetString(IDS_SYNC_LOGIN_INTRODUCTION));
    localized_strings.SetString(L"signinprefix",
        l10n_util::GetString(IDS_SYNC_LOGIN_SIGNIN_PREFIX));
    localized_strings.SetString(L"signinsuffix",
        l10n_util::GetString(IDS_SYNC_LOGIN_SIGNIN_SUFFIX));
    localized_strings.SetString(L"cannotbeblank",
        l10n_util::GetString(IDS_SYNC_CANNOT_BE_BLANK));
    localized_strings.SetString(L"emaillabel",
        l10n_util::GetString(IDS_SYNC_LOGIN_EMAIL));
    localized_strings.SetString(L"passwordlabel",
        l10n_util::GetString(IDS_SYNC_LOGIN_PASSWORD));
    localized_strings.SetString(L"invalidcredentials",
        l10n_util::GetString(IDS_SYNC_INVALID_USER_CREDENTIALS));
    localized_strings.SetString(L"signin",
        l10n_util::GetString(IDS_SYNC_SIGNIN));
    localized_strings.SetString(L"couldnotconnect",
        l10n_util::GetString(IDS_SYNC_LOGIN_COULD_NOT_CONNECT));
    localized_strings.SetString(L"cannotaccessaccount",
        l10n_util::GetString(IDS_SYNC_CANNOT_ACCESS_ACCOUNT));
    localized_strings.SetString(L"createaccount",
        l10n_util::GetString(IDS_SYNC_CREATE_ACCOUNT));
    localized_strings.SetString(L"cancel",
        l10n_util::GetString(IDS_CANCEL));
    localized_strings.SetString(L"settingup",
        l10n_util::GetString(IDS_SYNC_LOGIN_SETTING_UP));
    localized_strings.SetString(L"success",
        l10n_util::GetString(IDS_SYNC_SUCCESS));
    localized_strings.SetString(L"errorsigningin",
        l10n_util::GetString(IDS_SYNC_ERROR_SIGNING_IN));
    static const base::StringPiece html(ResourceBundle::GetSharedInstance()
        .GetRawDataResource(IDR_GAIA_LOGIN_HTML));
    SetFontAndTextDirection(&localized_strings);
    response = jstemplate_builder::GetI18nTemplateHtml(
        html, &localized_strings);
  } else if (path_raw == chrome::kSyncMergeAndSyncPath) {
    DictionaryValue localized_strings;
    localized_strings.SetString(L"introduction",
        l10n_util::GetString(IDS_SYNC_MERGE_INTRODUCTION));
    localized_strings.SetString(L"mergeandsynclabel",
        l10n_util::GetString(IDS_SYNC_MERGE_AND_SYNC_LABEL));
    localized_strings.SetString(L"abortlabel",
        l10n_util::GetString(IDS_ABORT));
    localized_strings.SetString(L"closelabel",
        l10n_util::GetString(IDS_CLOSE));
    localized_strings.SetString(L"mergeandsyncwarning",
        l10n_util::GetString(IDS_SYNC_MERGE_WARNING));
    localized_strings.SetString(L"setuperror",
                                l10n_util::GetString(IDS_SYNC_SETUP_ERROR));

    static const base::StringPiece html(ResourceBundle::GetSharedInstance()
        .GetRawDataResource(IDR_MERGE_AND_SYNC_HTML));
    SetFontAndTextDirection(&localized_strings);
    response = jstemplate_builder::GetI18nTemplateHtml(
        html, &localized_strings);
  } else if (path_raw == chrome::kSyncSetupDonePath) {
    DictionaryValue localized_strings;
    localized_strings.SetString(L"success",
        l10n_util::GetString(IDS_SYNC_SUCCESS));
    localized_strings.SetString(L"setupsummary",
        l10n_util::GetString(IDS_SYNC_SETUP_ALL_DONE));
    localized_strings.SetString(L"firsttimesetupsummary",
        l10n_util::GetString(IDS_SYNC_SETUP_FIRST_TIME_ALL_DONE));
    localized_strings.SetString(L"okay",
        l10n_util::GetString(IDS_SYNC_SETUP_OK_BUTTON_LABEL));
    static const base::StringPiece html(ResourceBundle::GetSharedInstance()
        .GetRawDataResource(IDR_SYNC_SETUP_DONE_HTML));
    SetFontAndTextDirection(&localized_strings);
    response = jstemplate_builder::GetI18nTemplateHtml(
        html, &localized_strings);
  } else if (path_raw == chrome::kSyncSetupFlowPath) {
    static const base::StringPiece html(ResourceBundle::GetSharedInstance()
        .GetRawDataResource(IDR_SYNC_SETUP_FLOW_HTML));
    response = html.as_string();
  }
  // Send the response.
  html_bytes->data.resize(response.size());
  std::copy(response.begin(), response.end(), html_bytes->data.begin());
  SendResponse(request_id, html_bytes);
}

SyncSetupWizard::SyncSetupWizard(ProfileSyncService* service)
    : service_(service),
      flow_container_(new SyncSetupFlowContainer()) {
  // Add our network layer data source for 'cloudy' URLs.
  SyncResourcesSource* sync_source = new SyncResourcesSource();
  bool posted = ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(Singleton<ChromeURLDataManager>::get(),
                        &ChromeURLDataManager::AddDataSource,
                        sync_source));
  if (!posted) {
    sync_source->AddRef();
    sync_source->Release();  // Keep Valgrind happy in unit tests.
  }
}

SyncSetupWizard::~SyncSetupWizard() {
  delete flow_container_;
}

void SyncSetupWizard::Step(State advance_state) {
  SyncSetupFlow* flow = flow_container_->get_flow();
  if (flow) {
    // A setup flow is in progress and dialog is currently showing.
    flow->Advance(advance_state);
  } else if (!service_->profile()->GetPrefs()->GetBoolean(
             prefs::kSyncHasSetupCompleted)) {
    if (IsTerminalState(advance_state))
      return;
    // No flow is in progress, and we have never escorted the user all the
    // way through the wizard flow.
    flow_container_->set_flow(
        SyncSetupFlow::Run(service_, flow_container_, advance_state, DONE));
  } else {
    // No flow in in progress, but we've finished the wizard flow once before.
    // This is just a discrete run.
    if (IsTerminalState(advance_state))
      return;  // Nothing to do.
    flow_container_->set_flow(SyncSetupFlow::Run(service_, flow_container_,
        advance_state, GetEndStateForDiscreteRun(advance_state)));
  }
}

// static
bool SyncSetupWizard::IsTerminalState(State advance_state) {
  return advance_state == GAIA_SUCCESS ||
         advance_state == DONE ||
         advance_state == DONE_FIRST_TIME ||
         advance_state == FATAL_ERROR;
}

bool SyncSetupWizard::IsVisible() const {
  return flow_container_->get_flow() != NULL;
}

// static
SyncSetupWizard::State SyncSetupWizard::GetEndStateForDiscreteRun(
    State start_state) {
  State result = start_state == GAIA_LOGIN ? GAIA_SUCCESS : DONE;
  DCHECK_NE(DONE, result) <<
      "Invalid start state for discrete run: " << start_state;
  return result;
}
