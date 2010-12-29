// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/remoting/setup_flow.h"

#include "app/gfx/font_util.h"
#include "app/l10n_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/remoting/remoting_resources_source.h"
#include "chrome/browser/remoting/setup_flow_login_step.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/pref_names.h"
#include "gfx/font.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

namespace remoting {

static const wchar_t kDoneIframeXPath[] = L"//iframe[@id='done']";

SetupFlowStep::SetupFlowStep() { }
SetupFlowStep::~SetupFlowStep() { }

SetupFlowStepBase::SetupFlowStepBase()
    : flow_(NULL) {
}

SetupFlowStepBase::~SetupFlowStepBase() { }

void SetupFlowStepBase::Start(SetupFlow* flow, DoneCallback* done_callback) {
  done_callback_.reset(done_callback);
  flow_ = flow;
  DoStart();
}

SetupFlowStep* SetupFlowStepBase::GetNextStep() {
  DCHECK(done_);
  return next_step_;
}

void SetupFlowStepBase::ExecuteJavascriptInIFrame(
    const std::wstring& iframe_xpath, const std::wstring& js) {
  DOMUI* dom_ui = flow()->dom_ui();
  DCHECK(dom_ui);

  RenderViewHost* rvh = dom_ui->tab_contents()->render_view_host();
  rvh->ExecuteJavascriptInWebFrame(iframe_xpath, js);
}

void SetupFlowStepBase::FinishStep(SetupFlowStep* next_step) {
  next_step_ = next_step;
  done_ = true;
  done_callback_->Run();
}

SetupFlowDoneStep::SetupFlowDoneStep() { }
SetupFlowDoneStep::~SetupFlowDoneStep() { }

void SetupFlowDoneStep::HandleMessage(const std::string& message,
                                      const ListValue* args) {
}

void SetupFlowDoneStep::Cancel() { }

void SetupFlowDoneStep::DoStart() {
  std::wstring javascript = L"setMessage('You are all set!');";
  ExecuteJavascriptInIFrame(kDoneIframeXPath, javascript);

  flow()->dom_ui()->CallJavascriptFunction(L"showSetupDone");

  ExecuteJavascriptInIFrame(kDoneIframeXPath, L"onPageShown();");
}

SetupFlow::SetupFlow(const std::string& args, Profile* profile,
                     SetupFlowStep* first_step)
    : dom_ui_(NULL),
      dialog_start_args_(args),
      profile_(profile),
      current_step_(first_step) {
  // TODO(hclam): The data source should be added once.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(ChromeURLDataManager::GetInstance(),
                        &ChromeURLDataManager::AddDataSource,
                        make_scoped_refptr(new RemotingResourcesSource())));
}

SetupFlow::~SetupFlow() { }

// static
SetupFlow* SetupFlow::OpenSetupDialog(Profile* profile) {
  // Set the arguments for showing the gaia login page.
  DictionaryValue args;
  args.SetString("iframeToShow", "login");
  args.SetString("user", "");
  args.SetInteger("error", 0);
  args.SetBoolean("editable_user", true);

  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  Browser* b = BrowserList::GetLastActive();
  if (!b)
    return NULL;

  SetupFlow *flow = new SetupFlow(json_args, profile, new SetupFlowLoginStep());
  b->BrowserShowHtmlDialog(flow, NULL);
  return flow;
}

GURL SetupFlow::GetDialogContentURL() const {
  return GURL("chrome://remotingresources/setup");
}

void SetupFlow::GetDOMMessageHandlers(
    std::vector<DOMMessageHandler*>* handlers) const {
  // The called will be responsible for deleting this object.
  handlers->push_back(const_cast<SetupFlow*>(this));
}

void SetupFlow::GetDialogSize(gfx::Size* size) const {
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
void SetupFlow::OnDialogClosed(const std::string& json_retval) {
  if (current_step_ != NULL)
    current_step_->Cancel();
}

std::string SetupFlow::GetDialogArgs() const {
    return dialog_start_args_;
}

void SetupFlow::OnCloseContents(TabContents* source,
                                bool* out_close_dialog) {
}

std::wstring  SetupFlow::GetDialogTitle() const {
  return l10n_util::GetString(IDS_REMOTING_SETUP_DIALOG_TITLE);
}

bool SetupFlow::IsDialogModal() const {
  return false;
}

bool SetupFlow::ShouldShowDialogTitle() const {
  return true;
}

DOMMessageHandler* SetupFlow::Attach(DOMUI* dom_ui) {
  dom_ui_ = dom_ui;
  StartCurrentStep();
  return DOMMessageHandler::Attach(dom_ui);
}

void SetupFlow::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("SubmitAuth",
      NewCallback(this, &SetupFlow::HandleSubmitAuth));
}

void SetupFlow::HandleSubmitAuth(const ListValue* args) {
  current_step_->HandleMessage("SubmitAuth", args);
}

void SetupFlow::StartCurrentStep() {
  current_step_->Start(this, NewCallback(this, &SetupFlow::OnStepDone));
}

void SetupFlow::OnStepDone() {
  current_step_.reset(current_step_->GetNextStep());
  StartCurrentStep();
}

}  // namespace remoting
