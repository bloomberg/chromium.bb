// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/autocheckout_manager.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/browser/autocheckout_request_manager.h"
#include "components/autofill/content/browser/autocheckout_steps.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_messages.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/web_element_descriptor.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/ssl_status.h"
#include "googleurl/src/gurl.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/gfx/rect.h"

using content::RenderViewHost;
using content::SSLStatus;
using content::WebContents;

namespace autofill {

namespace {

const char kGoogleAccountsUrl[] = "https://accounts.google.com/";

// Build FormFieldData based on the supplied |autocomplete_attribute|. Will
// fill rest of properties with default values.
FormFieldData BuildField(const std::string& autocomplete_attribute) {
  FormFieldData field;
  field.name = base::string16();
  field.value = base::string16();
  field.autocomplete_attribute = autocomplete_attribute;
  field.form_control_type = "text";
  return field;
}

// Build Autocheckout specific form data to be consumed by
// AutofillDialogController to show the Autocheckout specific UI.
FormData BuildAutocheckoutFormData() {
  FormData formdata;
  formdata.fields.push_back(BuildField("email"));
  formdata.fields.push_back(BuildField("cc-name"));
  formdata.fields.push_back(BuildField("cc-number"));
  formdata.fields.push_back(BuildField("cc-exp-month"));
  formdata.fields.push_back(BuildField("cc-exp-year"));
  formdata.fields.push_back(BuildField("cc-csc"));
  formdata.fields.push_back(BuildField("billing address-line1"));
  formdata.fields.push_back(BuildField("billing address-line2"));
  formdata.fields.push_back(BuildField("billing locality"));
  formdata.fields.push_back(BuildField("billing region"));
  formdata.fields.push_back(BuildField("billing country"));
  formdata.fields.push_back(BuildField("billing postal-code"));
  formdata.fields.push_back(BuildField("billing tel"));
  formdata.fields.push_back(BuildField("shipping name"));
  formdata.fields.push_back(BuildField("shipping address-line1"));
  formdata.fields.push_back(BuildField("shipping address-line2"));
  formdata.fields.push_back(BuildField("shipping locality"));
  formdata.fields.push_back(BuildField("shipping region"));
  formdata.fields.push_back(BuildField("shipping country"));
  formdata.fields.push_back(BuildField("shipping postal-code"));
  formdata.fields.push_back(BuildField("shipping tel"));
  return formdata;
}

AutofillMetrics::AutocheckoutBuyFlowMetric AutocheckoutStatusToUmaMetric(
    AutocheckoutStatus status) {
  switch (status) {
    case SUCCESS:
      return AutofillMetrics::AUTOCHECKOUT_BUY_FLOW_SUCCESS;
    case MISSING_FIELDMAPPING:
      return AutofillMetrics::AUTOCHECKOUT_BUY_FLOW_MISSING_FIELDMAPPING;
    case MISSING_CLICK_ELEMENT_BEFORE_FORM_FILLING:
      return AutofillMetrics::
          AUTOCHECKOUT_BUY_FLOW_MISSING_CLICK_ELEMENT_BEFORE_FORM_FILLING;
    case MISSING_CLICK_ELEMENT_AFTER_FORM_FILLING:
      return AutofillMetrics::
          AUTOCHECKOUT_BUY_FLOW_MISSING_CLICK_ELEMENT_AFTER_FORM_FILLING;
    case MISSING_ADVANCE:
      return AutofillMetrics::AUTOCHECKOUT_BUY_FLOW_MISSING_ADVANCE_ELEMENT;
    case CANNOT_PROCEED:
      return AutofillMetrics::AUTOCHECKOUT_BUY_FLOW_CANNOT_PROCEED;
    case AUTOCHECKOUT_STATUS_NUM_STATUS:
      NOTREACHED();
  }

  NOTREACHED();
  return AutofillMetrics::NUM_AUTOCHECKOUT_BUY_FLOW_METRICS;
}

// Callback for retrieving Google Account cookies. |callback| is passed the
// retrieved cookies and posted back to the UI thread. |cookies| is any Google
// Account cookies.
void GetGoogleCookiesCallback(
    const base::Callback<void(const std::string&)>& callback,
    const std::string& cookies) {
  content::BrowserThread::PostTask(content::BrowserThread::UI,
                                   FROM_HERE,
                                   base::Bind(callback, cookies));
}

// Gets Google Account cookies. Must be called on the IO thread.
// |request_context_getter| is a getter for the current request context.
// |callback| is called when retrieving cookies is completed.
void GetGoogleCookies(
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    const base::Callback<void(const std::string&)>& callback) {
  net::URLRequestContext* url_request_context =
      request_context_getter->GetURLRequestContext();
  if (!url_request_context)
    return;

  net::CookieStore* cookie_store = url_request_context->cookie_store();

  base::Callback<void(const std::string&)> cookie_callback = base::Bind(
      &GetGoogleCookiesCallback,
      callback);

  net::CookieOptions cookie_options;
  cookie_options.set_include_httponly();
  cookie_store->GetCookiesWithOptionsAsync(GURL(kGoogleAccountsUrl),
                                           cookie_options,
                                           cookie_callback);
}

bool IsBillingGroup(FieldTypeGroup group) {
  return group == AutofillType::ADDRESS_BILLING ||
         group == AutofillType::PHONE_BILLING;
}

const char kTransactionIdNotSet[] = "transaction id not set";

}  // namespace

AutocheckoutManager::AutocheckoutManager(AutofillManager* autofill_manager)
    : autofill_manager_(autofill_manager),
      metric_logger_(new AutofillMetrics),
      autocheckout_offered_(false),
      is_autocheckout_bubble_showing_(false),
      in_autocheckout_flow_(false),
      google_transaction_id_(kTransactionIdNotSet),
      weak_ptr_factory_(this) {}

AutocheckoutManager::~AutocheckoutManager() {
}

void AutocheckoutManager::FillForms() {
  // |page_meta_data_| should have been set by OnLoadedPageMetaData.
  DCHECK(page_meta_data_);

  // Fill the forms on the page with data given by user.
  std::vector<FormData> filled_forms;
  const std::vector<FormStructure*>& form_structures =
    autofill_manager_->GetFormStructures();
  for (std::vector<FormStructure*>::const_iterator iter =
           form_structures.begin(); iter != form_structures.end(); ++iter) {
    FormStructure* form_structure = *iter;
    form_structure->set_filled_by_autocheckout(true);
    FormData form = form_structure->ToFormData();
    DCHECK_EQ(form_structure->field_count(), form.fields.size());

    for (size_t i = 0; i < form_structure->field_count(); ++i) {
      const AutofillField* field = form_structure->field(i);
      SetValue(*field, &form.fields[i]);
    }

    filled_forms.push_back(form);
  }

  // Send filled forms along with proceed descriptor to renderer.
  RenderViewHost* host =
      autofill_manager_->GetWebContents()->GetRenderViewHost();
  if (!host)
    return;

  host->Send(new AutofillMsg_FillFormsAndClick(
      host->GetRoutingID(),
      filled_forms,
      page_meta_data_->click_elements_before_form_fill,
      page_meta_data_->click_elements_after_form_fill,
      page_meta_data_->proceed_element_descriptor));
}

void AutocheckoutManager::OnClickFailed(AutocheckoutStatus status) {
  DCHECK(in_autocheckout_flow_);
  DCHECK_NE(MISSING_FIELDMAPPING, status);

  SendAutocheckoutStatus(status);
  SetStepProgressForPage(page_meta_data_->current_page_number,
                         AUTOCHECKOUT_STEP_FAILED);

  autofill_manager_->delegate()->OnAutocheckoutError();
  in_autocheckout_flow_ = false;
}

void AutocheckoutManager::OnLoadedPageMetaData(
    scoped_ptr<AutocheckoutPageMetaData> page_meta_data) {
  scoped_ptr<AutocheckoutPageMetaData> old_meta_data =
      page_meta_data_.Pass();
  page_meta_data_ = page_meta_data.Pass();

  // Don't log that the bubble could be displayed if the user entered an
  // Autocheckout flow and sees the first page of the flow again due to an
  // error.
  if (IsStartOfAutofillableFlow() && !in_autocheckout_flow_) {
    metric_logger_->LogAutocheckoutBubbleMetric(
        AutofillMetrics::BUBBLE_COULD_BE_DISPLAYED);
  }

  // On the first page of an Autocheckout flow, when this function is called the
  // user won't have opted into the flow yet.
  if (!in_autocheckout_flow_)
    return;

  AutocheckoutStatus status = SUCCESS;

  // Missing Autofill server results.
  if (!page_meta_data_) {
    in_autocheckout_flow_ = false;
    status = MISSING_FIELDMAPPING;
  } else if (page_meta_data_->IsStartOfAutofillableFlow()) {
    // Not possible unless Autocheckout failed to proceed.
    in_autocheckout_flow_ = false;
    status = CANNOT_PROCEED;
  } else if (!page_meta_data_->IsInAutofillableFlow()) {
    // Missing Autocheckout meta data in the Autofill server results.
    in_autocheckout_flow_ = false;
    status = MISSING_FIELDMAPPING;
  } else if (page_meta_data_->current_page_number <=
                 old_meta_data->current_page_number) {
    // Not possible unless Autocheckout failed to proceed.
    in_autocheckout_flow_ = false;
    status = CANNOT_PROCEED;
  }

  // Encountered an error during the Autocheckout flow, probably to
  // do with a problem on the previous page.
  if (!in_autocheckout_flow_) {
    if (old_meta_data) {
      SetStepProgressForPage(old_meta_data->current_page_number,
                             AUTOCHECKOUT_STEP_FAILED);
    }
    SendAutocheckoutStatus(status);
    autofill_manager_->delegate()->OnAutocheckoutError();
    return;
  }

  SetStepProgressForPage(old_meta_data->current_page_number,
                         AUTOCHECKOUT_STEP_COMPLETED);
  SetStepProgressForPage(page_meta_data_->current_page_number,
                         AUTOCHECKOUT_STEP_STARTED);

  FillForms();
  // If the current page is the last page in the flow, set in-progress
  // steps to 'completed', and send status.
  if (page_meta_data_->IsEndOfAutofillableFlow()) {
    SetStepProgressForPage(page_meta_data_->current_page_number,
                           AUTOCHECKOUT_STEP_COMPLETED);
    SendAutocheckoutStatus(status);
    autofill_manager_->delegate()->OnAutocheckoutSuccess();
    in_autocheckout_flow_ = false;
  }
}

void AutocheckoutManager::OnFormsSeen() {
  autocheckout_offered_ = false;
}

bool AutocheckoutManager::ShouldIgnoreAjax() {
  return in_autocheckout_flow_ && page_meta_data_->ignore_ajax;
}

void AutocheckoutManager::MaybeShowAutocheckoutBubble(
    const GURL& frame_url,
    const content::SSLStatus& ssl_status,
    const gfx::RectF& bounding_box) {
  if (autocheckout_offered_ ||
      is_autocheckout_bubble_showing_ ||
      !IsStartOfAutofillableFlow())
    return;

  base::Callback<void(const std::string&)> callback = base::Bind(
      &AutocheckoutManager::ShowAutocheckoutBubble,
      weak_ptr_factory_.GetWeakPtr(),
      frame_url,
      ssl_status,
      bounding_box);

  content::WebContents* web_contents = autofill_manager_->GetWebContents();
  if (!web_contents)
    return;

  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  if(!browser_context)
    return;

  scoped_refptr<net::URLRequestContextGetter> request_context =
      scoped_refptr<net::URLRequestContextGetter>(
          browser_context->GetRequestContext());

  if (!request_context.get())
    return;

  base::Closure task = base::Bind(&GetGoogleCookies, request_context, callback);

  content::BrowserThread::PostTask(content::BrowserThread::IO,
                                   FROM_HERE,
                                   task);
}

void AutocheckoutManager::ReturnAutocheckoutData(
    const FormStructure* result,
    const std::string& google_transaction_id) {
  if (!result) {
    in_autocheckout_flow_ = false;
    return;
  }

  google_transaction_id_ = google_transaction_id;
  in_autocheckout_flow_ = true;
  metric_logger_->LogAutocheckoutBuyFlowMetric(
      AutofillMetrics::AUTOCHECKOUT_BUY_FLOW_STARTED);

  profile_.reset(new AutofillProfile());
  credit_card_.reset(new CreditCard());
  billing_address_.reset(new AutofillProfile());

  for (size_t i = 0; i < result->field_count(); ++i) {
    AutofillFieldType type = result->field(i)->type();
    const base::string16& value = result->field(i)->value;
    if (type == CREDIT_CARD_VERIFICATION_CODE) {
      cvv_ = result->field(i)->value;
      continue;
    }
    FieldTypeGroup group = AutofillType(type).group();
    if (group == AutofillType::CREDIT_CARD) {
      credit_card_->SetRawInfo(type, value);
    } else if (type == ADDRESS_HOME_COUNTRY) {
      profile_->SetInfo(type, value, autofill_manager_->app_locale());
    } else if (type == ADDRESS_BILLING_COUNTRY) {
      billing_address_->SetInfo(type, value, autofill_manager_->app_locale());
    } else if (IsBillingGroup(group)) {
      billing_address_->SetRawInfo(type, value);
    } else {
      profile_->SetRawInfo(type, value);
    }
  }

  // Page types only available in first-page meta data, so save
  // them for use later as we navigate.
  page_types_ = page_meta_data_->page_types;
  SetStepProgressForPage(page_meta_data_->current_page_number,
                         AUTOCHECKOUT_STEP_STARTED);

  FillForms();

  // If the current page is the last page in the flow, set in-progress
  // steps to 'completed', and send status.
  if (page_meta_data_->IsEndOfAutofillableFlow()) {
    SetStepProgressForPage(page_meta_data_->current_page_number,
                           AUTOCHECKOUT_STEP_COMPLETED);
    SendAutocheckoutStatus(SUCCESS);
    autofill_manager_->delegate()->OnAutocheckoutSuccess();
    in_autocheckout_flow_ = false;
  }
}

void AutocheckoutManager::set_metric_logger(
    scoped_ptr<AutofillMetrics> metric_logger) {
  metric_logger_ = metric_logger.Pass();
}

void AutocheckoutManager::MaybeShowAutocheckoutDialog(
    const GURL& frame_url,
    const SSLStatus& ssl_status,
    bool show_dialog) {
  is_autocheckout_bubble_showing_ = false;
  if (!show_dialog)
    return;

  FormData form = BuildAutocheckoutFormData();
  form.ssl_status = ssl_status;

  base::Callback<void(const FormStructure*, const std::string&)> callback =
      base::Bind(&AutocheckoutManager::ReturnAutocheckoutData,
                 weak_ptr_factory_.GetWeakPtr());
  autofill_manager_->ShowRequestAutocompleteDialog(
      form, frame_url, DIALOG_TYPE_AUTOCHECKOUT, callback);

  for (std::map<int, std::vector<AutocheckoutStepType> >::const_iterator
          it = page_meta_data_->page_types.begin();
      it != page_meta_data_->page_types.end(); ++it) {
    for (size_t i = 0; i < it->second.size(); ++i) {
      autofill_manager_->delegate()->AddAutocheckoutStep(it->second[i]);
    }
  }
}

void AutocheckoutManager::ShowAutocheckoutBubble(
    const GURL& frame_url,
    const content::SSLStatus& ssl_status,
    const gfx::RectF& bounding_box,
    const std::string& cookies) {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::Callback<void(bool)> callback = base::Bind(
      &AutocheckoutManager::MaybeShowAutocheckoutDialog,
      weak_ptr_factory_.GetWeakPtr(),
      frame_url,
      ssl_status);
  autofill_manager_->delegate()->ShowAutocheckoutBubble(
      bounding_box,
      cookies.find("LSID") != std::string::npos,
      callback);
  is_autocheckout_bubble_showing_ = true;
  autocheckout_offered_ = true;
}

bool AutocheckoutManager::IsStartOfAutofillableFlow() const {
  return page_meta_data_ && page_meta_data_->IsStartOfAutofillableFlow();
}

bool AutocheckoutManager::IsInAutofillableFlow() const {
  return page_meta_data_ && page_meta_data_->IsInAutofillableFlow();
}

void AutocheckoutManager::SetValue(const AutofillField& field,
                                   FormFieldData* field_to_fill) {
  // No-op if Autofill server doesn't know about the field.
  if (field.server_type() == NO_SERVER_DATA)
    return;

  AutofillFieldType type = field.type();

  if (type == FIELD_WITH_DEFAULT_VALUE) {
    DCHECK(field.is_checkable);
    // For a form with radio buttons, like:
    // <form>
    //   <input type="radio" name="sex" value="male">Male<br>
    //   <input type="radio" name="sex" value="female">Female
    // </form>
    // If the default value specified at the server is "female", then
    // Autofill server responds back with following field mappings
    //   (fieldtype: FIELD_WITH_DEFAULT_VALUE, value: "female")
    //   (fieldtype: FIELD_WITH_DEFAULT_VALUE, value: "female")
    // Note that, the field mapping is repeated twice to respond to both the
    // input elements with the same name/signature in the form.
    base::string16 default_value = UTF8ToUTF16(field.default_value());
    // Mark the field checked if server says the default value of the field
    // to be this field's value.
    field_to_fill->is_checked = (field.value == default_value);
    return;
  }

  // Handle verification code directly.
  if (type == CREDIT_CARD_VERIFICATION_CODE) {
    field_to_fill->value = cvv_;
    return;
  }

  if (AutofillType(type).group() == AutofillType::CREDIT_CARD) {
    credit_card_->FillFormField(
        field, 0, autofill_manager_->app_locale(), field_to_fill);
  } else if (IsBillingGroup(AutofillType(type).group())) {
    billing_address_->FillFormField(
        field, 0, autofill_manager_->app_locale(), field_to_fill);
  } else {
    profile_->FillFormField(
        field, 0, autofill_manager_->app_locale(), field_to_fill);
  }
}

void AutocheckoutManager::SendAutocheckoutStatus(AutocheckoutStatus status) {
  // To ensure stale data isn't being sent.
  DCHECK_NE(kTransactionIdNotSet, google_transaction_id_);

  AutocheckoutRequestManager::CreateForBrowserContext(
      autofill_manager_->GetWebContents()->GetBrowserContext());
  AutocheckoutRequestManager* autocheckout_request_manager =
      AutocheckoutRequestManager::FromBrowserContext(
          autofill_manager_->GetWebContents()->GetBrowserContext());
  // It is assumed that the domain Autocheckout starts on does not change
  // during the flow.  If this proves to be incorrect, the |source_url| from
  // AutofillDialogControllerImpl will need to be provided in its callback in
  // addition to the Google transaction id.
  autocheckout_request_manager->SendAutocheckoutStatus(
      status,
      autofill_manager_->GetWebContents()->GetURL(),
      google_transaction_id_);

  // Log the result of this Autocheckout flow to UMA.
  metric_logger_->LogAutocheckoutBuyFlowMetric(
      AutocheckoutStatusToUmaMetric(status));

  google_transaction_id_ = kTransactionIdNotSet;
}

void AutocheckoutManager::SetStepProgressForPage(
    int page_number,
    AutocheckoutStepStatus status) {
  if (page_types_.count(page_number) == 1) {
    for (size_t i = 0; i < page_types_[page_number].size(); ++i) {
      autofill_manager_->delegate()->UpdateAutocheckoutStep(
          page_types_[page_number][i], status);
    }
  }
}

}  // namespace autofill
