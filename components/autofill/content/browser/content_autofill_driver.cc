// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver.h"

#include <utility>

#include "base/command_line.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "ui/gfx/geometry/size_f.h"

namespace autofill {

ContentAutofillDriver::ContentAutofillDriver(
    content::RenderFrameHost* render_frame_host,
    AutofillClient* client,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager)
    : render_frame_host_(render_frame_host),
      autofill_manager_(new AutofillManager(this,
                                            client,
                                            app_locale,
                                            enable_download_manager)),
      autofill_external_delegate_(autofill_manager_.get(), this),
      key_press_handler_manager_(this),
      binding_(this) {
  autofill_manager_->SetExternalDelegate(&autofill_external_delegate_);
}

ContentAutofillDriver::~ContentAutofillDriver() {}

// static
ContentAutofillDriver* ContentAutofillDriver::GetForRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  ContentAutofillDriverFactory* factory =
      ContentAutofillDriverFactory::FromWebContents(
          content::WebContents::FromRenderFrameHost(render_frame_host));
  return factory ? factory->DriverForFrame(render_frame_host) : nullptr;
}

void ContentAutofillDriver::BindRequest(mojom::AutofillDriverRequest request) {
  binding_.Bind(std::move(request));
}

bool ContentAutofillDriver::IsIncognito() const {
  return render_frame_host_->GetSiteInstance()
      ->GetBrowserContext()
      ->IsOffTheRecord();
}

net::URLRequestContextGetter* ContentAutofillDriver::GetURLRequestContext() {
  return content::BrowserContext::GetDefaultStoragePartition(
      render_frame_host_->GetSiteInstance()->GetBrowserContext())->
          GetURLRequestContext();
}

bool ContentAutofillDriver::RendererIsAvailable() {
  return render_frame_host_->GetRenderViewHost() != NULL;
}

void ContentAutofillDriver::SendFormDataToRenderer(
    int query_id,
    RendererFormDataAction action,
    const FormData& data) {
  if (!RendererIsAvailable())
    return;

  switch (action) {
    case FORM_DATA_ACTION_FILL:
      GetAutofillAgent()->FillForm(query_id, data);
      break;
    case FORM_DATA_ACTION_PREVIEW:
      GetAutofillAgent()->PreviewForm(query_id, data);
      break;
  }
}

void ContentAutofillDriver::PropagateAutofillPredictions(
    const std::vector<FormStructure*>& forms) {
  autofill_manager_->client()->PropagateAutofillPredictions(render_frame_host_,
                                                            forms);
}

void ContentAutofillDriver::SendAutofillTypePredictionsToRenderer(
    const std::vector<FormStructure*>& forms) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kShowAutofillTypePredictions))
    return;

  if (!RendererIsAvailable())
    return;

  std::vector<FormDataPredictions> type_predictions =
      FormStructure::GetFieldTypePredictions(forms);
  GetAutofillAgent()->FieldTypePredictionsAvailable(type_predictions);
}

void ContentAutofillDriver::RendererShouldAcceptDataListSuggestion(
    const base::string16& value) {
  if (!RendererIsAvailable())
    return;
  GetAutofillAgent()->AcceptDataListSuggestion(value);
}

void ContentAutofillDriver::RendererShouldClearFilledForm() {
  if (!RendererIsAvailable())
    return;
  GetAutofillAgent()->ClearForm();
}

void ContentAutofillDriver::RendererShouldClearPreviewedForm() {
  if (!RendererIsAvailable())
    return;
  GetAutofillAgent()->ClearPreviewedForm();
}

void ContentAutofillDriver::RendererShouldFillFieldWithValue(
    const base::string16& value) {
  if (!RendererIsAvailable())
    return;
  GetAutofillAgent()->FillFieldWithValue(value);
}

void ContentAutofillDriver::RendererShouldPreviewFieldWithValue(
    const base::string16& value) {
  if (!RendererIsAvailable())
    return;
  GetAutofillAgent()->PreviewFieldWithValue(value);
}

void ContentAutofillDriver::PopupHidden() {
  // If the unmask prompt is showing, keep showing the preview. The preview
  // will be cleared when the prompt closes.
  if (!autofill_manager_->IsShowingUnmaskPrompt())
    RendererShouldClearPreviewedForm();
}

gfx::RectF ContentAutofillDriver::TransformBoundingBoxToViewportCoordinates(
    const gfx::RectF& bounding_box) {
  content::RenderWidgetHostView* view = render_frame_host_->GetView();
  if (!view)
    return bounding_box;

  gfx::Point orig_point(bounding_box.x(), bounding_box.y());
  gfx::Point transformed_point =
      view->TransformPointToRootCoordSpace(orig_point);
  return gfx::RectF(transformed_point.x(), transformed_point.y(),
                    bounding_box.width(), bounding_box.height());
}

void ContentAutofillDriver::DidInteractWithCreditCardForm() {
  // Notify the WebContents about credit card inputs on HTTP pages.
  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  if (contents->GetVisibleURL().SchemeIsCryptographic())
    return;
  contents->OnCreditCardInputShownOnHttp();
}

void ContentAutofillDriver::FormsSeen(const std::vector<FormData>& forms,
                                      base::TimeTicks timestamp) {
  autofill_manager_->OnFormsSeen(forms, timestamp);
}

void ContentAutofillDriver::WillSubmitForm(const FormData& form,
                                           base::TimeTicks timestamp) {
  autofill_manager_->OnWillSubmitForm(form, timestamp);
}

void ContentAutofillDriver::FormSubmitted(const FormData& form) {
  autofill_manager_->OnFormSubmitted(form);
}

void ContentAutofillDriver::TextFieldDidChange(const FormData& form,
                                               const FormFieldData& field,
                                               base::TimeTicks timestamp) {
  autofill_manager_->OnTextFieldDidChange(form, field, timestamp);
}

void ContentAutofillDriver::QueryFormFieldAutofill(
    int32_t id,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  autofill_manager_->OnQueryFormFieldAutofill(id, form, field, bounding_box);
}

void ContentAutofillDriver::HidePopup() {
  autofill_manager_->OnHidePopup();
}

void ContentAutofillDriver::FocusNoLongerOnForm() {
  autofill_manager_->OnFocusNoLongerOnForm();
}

void ContentAutofillDriver::DidFillAutofillFormData(const FormData& form,
                                                    base::TimeTicks timestamp) {
  autofill_manager_->OnDidFillAutofillFormData(form, timestamp);
}

void ContentAutofillDriver::DidPreviewAutofillFormData() {
  autofill_manager_->OnDidPreviewAutofillFormData();
}

void ContentAutofillDriver::DidEndTextFieldEditing() {
  autofill_manager_->OnDidEndTextFieldEditing();
}

void ContentAutofillDriver::SetDataList(
    const std::vector<base::string16>& values,
    const std::vector<base::string16>& labels) {
  autofill_manager_->OnSetDataList(values, labels);
}

void ContentAutofillDriver::DidNavigateFrame(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame() &&
      !navigation_handle->IsSameDocument()) {
    autofill_manager_->Reset();
  }
}

void ContentAutofillDriver::SetAutofillManager(
    std::unique_ptr<AutofillManager> manager) {
  autofill_manager_ = std::move(manager);
  autofill_manager_->SetExternalDelegate(&autofill_external_delegate_);
}

const mojom::AutofillAgentPtr& ContentAutofillDriver::GetAutofillAgent() {
  // Here is a lazy binding, and will not reconnect after connection error.
  if (!autofill_agent_) {
    render_frame_host_->GetRemoteInterfaces()->GetInterface(
        mojo::MakeRequest(&autofill_agent_));
  }

  return autofill_agent_;
}

void ContentAutofillDriver::RegisterKeyPressHandler(
    const content::RenderWidgetHost::KeyPressEventCallback& handler) {
  key_press_handler_manager_.RegisterKeyPressHandler(handler);
}

void ContentAutofillDriver::RemoveKeyPressHandler() {
  key_press_handler_manager_.RemoveKeyPressHandler();
}

void ContentAutofillDriver::AddHandler(
    const content::RenderWidgetHost::KeyPressEventCallback& handler) {
  content::RenderWidgetHostView* view = render_frame_host_->GetView();
  if (!view)
    return;
  view->GetRenderWidgetHost()->AddKeyPressEventCallback(handler);
}

void ContentAutofillDriver::RemoveHandler(
    const content::RenderWidgetHost::KeyPressEventCallback& handler) {
  content::RenderWidgetHostView* view = render_frame_host_->GetView();
  if (!view)
    return;
  view->GetRenderWidgetHost()->RemoveKeyPressEventCallback(handler);
}

}  // namespace autofill
