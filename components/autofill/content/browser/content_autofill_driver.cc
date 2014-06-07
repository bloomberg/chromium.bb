// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver.h"

#include "base/command_line.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "ipc/ipc_message_macros.h"

namespace autofill {

namespace {

const char kContentAutofillDriverWebContentsUserDataKey[] =
    "web_contents_autofill_driver_impl";

}  // namespace

// static
void ContentAutofillDriver::CreateForWebContentsAndDelegate(
    content::WebContents* contents,
    AutofillClient* client,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager) {
  if (FromWebContents(contents))
    return;

  contents->SetUserData(
      kContentAutofillDriverWebContentsUserDataKey,
      new ContentAutofillDriver(
          contents, client, app_locale, enable_download_manager));
}

// static
ContentAutofillDriver* ContentAutofillDriver::FromWebContents(
    content::WebContents* contents) {
  return static_cast<ContentAutofillDriver*>(
      contents->GetUserData(kContentAutofillDriverWebContentsUserDataKey));
}

ContentAutofillDriver::ContentAutofillDriver(
    content::WebContents* web_contents,
    AutofillClient* client,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager)
    : content::WebContentsObserver(web_contents),
      autofill_manager_(new AutofillManager(this,
                                            client,
                                            app_locale,
                                            enable_download_manager)),
      autofill_external_delegate_(autofill_manager_.get(), this),
      request_autocomplete_manager_(this) {
  autofill_manager_->SetExternalDelegate(&autofill_external_delegate_);
}

ContentAutofillDriver::~ContentAutofillDriver() {}

bool ContentAutofillDriver::IsOffTheRecord() const {
  return web_contents()->GetBrowserContext()->IsOffTheRecord();
}

net::URLRequestContextGetter* ContentAutofillDriver::GetURLRequestContext() {
  return web_contents()->GetBrowserContext()->GetRequestContext();
}

content::WebContents* ContentAutofillDriver::GetWebContents() {
  return web_contents();
}

base::SequencedWorkerPool* ContentAutofillDriver::GetBlockingPool() {
  return content::BrowserThread::GetBlockingPool();
}

bool ContentAutofillDriver::RendererIsAvailable() {
  return (web_contents()->GetRenderViewHost() != NULL);
}

void ContentAutofillDriver::SendFormDataToRenderer(
    int query_id,
    RendererFormDataAction action,
    const FormData& data) {
  if (!RendererIsAvailable())
    return;
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  switch (action) {
    case FORM_DATA_ACTION_FILL:
      host->Send(
          new AutofillMsg_FillForm(host->GetRoutingID(), query_id, data));
      break;
    case FORM_DATA_ACTION_PREVIEW:
      host->Send(
          new AutofillMsg_PreviewForm(host->GetRoutingID(), query_id, data));
      break;
  }
}

void ContentAutofillDriver::PingRenderer() {
  if (!RendererIsAvailable())
    return;
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  host->Send(new AutofillMsg_Ping(host->GetRoutingID()));
}

void ContentAutofillDriver::SendAutofillTypePredictionsToRenderer(
    const std::vector<FormStructure*>& forms) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kShowAutofillTypePredictions))
    return;

  if (!RendererIsAvailable())
    return;
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();

  std::vector<FormDataPredictions> type_predictions;
  FormStructure::GetFieldTypePredictions(forms, &type_predictions);
  host->Send(new AutofillMsg_FieldTypePredictionsAvailable(host->GetRoutingID(),
                                                           type_predictions));
}

void ContentAutofillDriver::RendererShouldAcceptDataListSuggestion(
    const base::string16& value) {
  if (!RendererIsAvailable())
    return;
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  host->Send(
      new AutofillMsg_AcceptDataListSuggestion(host->GetRoutingID(), value));
}

void ContentAutofillDriver::RendererShouldClearFilledForm() {
  if (!RendererIsAvailable())
    return;
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  host->Send(new AutofillMsg_ClearForm(host->GetRoutingID()));
}

void ContentAutofillDriver::RendererShouldClearPreviewedForm() {
  if (!RendererIsAvailable())
    return;
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  host->Send(new AutofillMsg_ClearPreviewedForm(host->GetRoutingID()));
}

void ContentAutofillDriver::RendererShouldFillFieldWithValue(
    const base::string16& value) {
  if (!RendererIsAvailable())
    return;
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  host->Send(new AutofillMsg_FillFieldWithValue(host->GetRoutingID(), value));
}
void ContentAutofillDriver::RendererShouldPreviewFieldWithValue(
    const base::string16& value) {
  if (!RendererIsAvailable())
    return;
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  host->Send(new AutofillMsg_PreviewFieldWithValue(host->GetRoutingID(),
                                                   value));
}

bool ContentAutofillDriver::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ContentAutofillDriver, message)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_FormsSeen,
                      autofill_manager_.get(),
                      AutofillManager::OnFormsSeen)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_FormSubmitted,
                      autofill_manager_.get(),
                      AutofillManager::OnFormSubmitted)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_TextFieldDidChange,
                      autofill_manager_.get(),
                      AutofillManager::OnTextFieldDidChange)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_QueryFormFieldAutofill,
                      autofill_manager_.get(),
                      AutofillManager::OnQueryFormFieldAutofill)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_DidPreviewAutofillFormData,
                      autofill_manager_.get(),
                      AutofillManager::OnDidPreviewAutofillFormData)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_PingAck,
                      &autofill_external_delegate_,
                      AutofillExternalDelegate::OnPingAck)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_DidFillAutofillFormData,
                      autofill_manager_.get(),
                      AutofillManager::OnDidFillAutofillFormData)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_DidEndTextFieldEditing,
                      autofill_manager_.get(),
                      AutofillManager::OnDidEndTextFieldEditing)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_HidePopup,
                      autofill_manager_.get(),
                      AutofillManager::OnHidePopup)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_SetDataList,
                      autofill_manager_.get(),
                      AutofillManager::OnSetDataList)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_RequestAutocomplete,
                      &request_autocomplete_manager_,
                      RequestAutocompleteManager::OnRequestAutocomplete)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_CancelRequestAutocomplete,
                      &request_autocomplete_manager_,
                      RequestAutocompleteManager::OnCancelRequestAutocomplete)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ContentAutofillDriver::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (details.is_navigation_to_different_page())
    autofill_manager_->Reset();
}

void ContentAutofillDriver::SetAutofillManager(
    scoped_ptr<AutofillManager> manager) {
  autofill_manager_ = manager.Pass();
  autofill_manager_->SetExternalDelegate(&autofill_external_delegate_);
}

void ContentAutofillDriver::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  autofill_manager_->client()->HideAutofillPopup();
}

void ContentAutofillDriver::WasHidden() {
  autofill_manager_->client()->HideAutofillPopup();
}

}  // namespace autofill
