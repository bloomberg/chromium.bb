// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/password_generation_agent.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/password_form_conversion_utils.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "content/public/renderer/render_view.h"
#include "google_apis/gaia/gaia_urls.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/gfx/rect.h"

namespace autofill {

namespace {

// Returns true if we think that this form is for account creation. |passwords|
// is filled with the password field(s) in the form.
bool GetAccountCreationPasswordFields(
    const blink::WebFormElement& form,
    std::vector<blink::WebInputElement>* passwords) {
  // Grab all of the passwords for the form.
  blink::WebVector<blink::WebFormControlElement> control_elements;
  form.getFormControlElements(control_elements);

  size_t num_input_elements = 0;
  for (size_t i = 0; i < control_elements.size(); i++) {
    blink::WebInputElement* input_element =
        toWebInputElement(&control_elements[i]);
    // Only pay attention to visible password fields.
    if (input_element &&
        input_element->isTextField() &&
        input_element->hasNonEmptyBoundingBox()) {
      num_input_elements++;
      if (input_element->isPasswordField())
        passwords->push_back(*input_element);
    }
  }

  // This may be too lenient, but we assume that any form with at least three
  // input elements where at least one of them is a password is an account
  // creation form.
  if (!passwords->empty() && num_input_elements >= 3) {
    // We trim |passwords| because occasionally there are forms where the
    // security question answers are put in password fields and we don't want
    // to fill those.
    if (passwords->size() > 2)
      passwords->resize(2);

    return true;
  }

  return false;
}

bool ContainsURL(const std::vector<GURL>& urls, const GURL& url) {
  return std::find(urls.begin(), urls.end(), url) != urls.end();
}

// Returns true if the |form1| is essentially equal to |form2|.
bool FormsAreEqual(const autofill::FormData& form1,
                   const PasswordForm& form2) {
  // TODO(zysxqn): use more signals than just origin to compare.
  // Note that FormData strips the fragement from the url while PasswordForm
  // strips both the fragement and the path, so we can't just compare these
  // two directly.
  return form1.origin.GetOrigin() == form2.origin.GetOrigin();
}

bool ContainsForm(const std::vector<autofill::FormData>& forms,
                  const PasswordForm& form) {
  for (std::vector<autofill::FormData>::const_iterator it =
           forms.begin(); it != forms.end(); ++it) {
    if (FormsAreEqual(*it, form))
      return true;
  }
  return false;
}

}  // namespace

PasswordGenerationAgent::PasswordGenerationAgent(
    content::RenderView* render_view)
    : content::RenderViewObserver(render_view),
      render_view_(render_view),
      password_is_generated_(false),
      password_edited_(false),
      enabled_(password_generation::IsPasswordGenerationEnabled()) {
  DVLOG(2) << "Password Generation is " << (enabled_ ? "Enabled" : "Disabled");
}
PasswordGenerationAgent::~PasswordGenerationAgent() {}

void PasswordGenerationAgent::DidFinishDocumentLoad(
    blink::WebLocalFrame* frame) {
  // In every navigation, the IPC message sent by the password autofill manager
  // to query whether the current form is blacklisted or not happens when the
  // document load finishes, so we need to clear previous states here before we
  // hear back from the browser. We only clear this state on main frame load
  // as we don't want subframe loads to clear state that we have received from
  // the main frame. Note that we assume there is only one account creation
  // form, but there could be multiple password forms in each frame.
  if (!frame->parent()) {
    not_blacklisted_password_form_origins_.clear();
    generation_enabled_forms_.clear();
    generation_element_.reset();
    possible_account_creation_form_.reset(new PasswordForm());
    password_elements_.clear();
    password_is_generated_ = false;
    if (password_edited_) {
      password_generation::LogPasswordGenerationEvent(
          password_generation::PASSWORD_EDITED);
    }
    password_edited_ = false;
  }
}

void PasswordGenerationAgent::DidFinishLoad(blink::WebLocalFrame* frame) {
  if (!enabled_)
    return;

  // We don't want to generate passwords if the browser won't store or sync
  // them.
  if (!ShouldAnalyzeDocument(frame->document()))
    return;

  blink::WebVector<blink::WebFormElement> forms;
  frame->document().forms(forms);
  for (size_t i = 0; i < forms.size(); ++i) {
    if (forms[i].isNull())
      continue;

    // If we can't get a valid PasswordForm, we skip this form because the
    // the password won't get saved even if we generate it.
    scoped_ptr<PasswordForm> password_form(
        CreatePasswordForm(forms[i]));
    if (!password_form.get()) {
      DVLOG(2) << "Skipping form as it would not be saved";
      continue;
    }

    // Do not generate password for GAIA since it is used to retrieve the
    // generated paswords.
    GURL realm(password_form->signon_realm);
    if (realm == GaiaUrls::GetInstance()->gaia_login_form_realm())
      continue;

    std::vector<blink::WebInputElement> passwords;
    if (GetAccountCreationPasswordFields(forms[i], &passwords)) {
      DVLOG(2) << "Account creation form detected";
      password_generation::LogPasswordGenerationEvent(
          password_generation::SIGN_UP_DETECTED);
      password_elements_ = passwords;
      possible_account_creation_form_.swap(password_form);
      DetermineGenerationElement();
      // We assume that there is only one account creation field per URL.
      return;
    }
  }
  password_generation::LogPasswordGenerationEvent(
      password_generation::NO_SIGN_UP_DETECTED);
}

bool PasswordGenerationAgent::ShouldAnalyzeDocument(
    const blink::WebDocument& document) const {
  // Make sure that this security origin is allowed to use password manager.
  // Generating a password that can't be saved is a bad idea.
  blink::WebSecurityOrigin origin = document.securityOrigin();
  if (!origin.canAccessPasswordManager()) {
    DVLOG(1) << "No PasswordManager access";
    return false;
  }

  return true;
}

bool PasswordGenerationAgent::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PasswordGenerationAgent, message)
    IPC_MESSAGE_HANDLER(AutofillMsg_FormNotBlacklisted,
                        OnFormNotBlacklisted)
    IPC_MESSAGE_HANDLER(AutofillMsg_GeneratedPasswordAccepted,
                        OnPasswordAccepted)
    IPC_MESSAGE_HANDLER(AutofillMsg_AccountCreationFormsDetected,
                        OnAccountCreationFormsDetected)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PasswordGenerationAgent::OnFormNotBlacklisted(const PasswordForm& form) {
  not_blacklisted_password_form_origins_.push_back(form.origin);
  DetermineGenerationElement();
}

void PasswordGenerationAgent::OnPasswordAccepted(
    const base::string16& password) {
  password_is_generated_ = true;
  password_generation::LogPasswordGenerationEvent(
      password_generation::PASSWORD_ACCEPTED);
  for (std::vector<blink::WebInputElement>::iterator it =
           password_elements_.begin();
       it != password_elements_.end(); ++it) {
    it->setValue(password);
    it->setAutofilled(true);
    // Advance focus to the next input field. We assume password fields in
    // an account creation form are always adjacent.
    render_view_->GetWebView()->advanceFocus(false);
  }
}

void PasswordGenerationAgent::OnAccountCreationFormsDetected(
    const std::vector<autofill::FormData>& forms) {
  generation_enabled_forms_.insert(
      generation_enabled_forms_.end(), forms.begin(), forms.end());
  DetermineGenerationElement();
}

void PasswordGenerationAgent::DetermineGenerationElement() {
  // Make sure local heuristics have identified a possible account creation
  // form.
  if (!possible_account_creation_form_.get() || password_elements_.empty()) {
    DVLOG(2) << "Local hueristics have not detected a possible account "
             << "creation form";
    return;
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kLocalHeuristicsOnlyForPasswordGeneration)) {
    DVLOG(2) << "Bypassing additional checks.";
  } else if (not_blacklisted_password_form_origins_.empty() ||
             !ContainsURL(not_blacklisted_password_form_origins_,
                          possible_account_creation_form_->origin)) {
    DVLOG(2) << "Have not received confirmation that password form isn't "
             << "blacklisted";
    return;
  } else if (generation_enabled_forms_.empty() ||
             !ContainsForm(generation_enabled_forms_,
                           *possible_account_creation_form_)) {
    // Note that this message will never be sent if this feature is disabled
    // (e.g. Password saving is disabled).
    DVLOG(2) << "Have not received confirmation from Autofill that form is "
             << "used for account creation";
    return;
  }

  DVLOG(2) << "Password generation eligible form found";
  generation_element_ = password_elements_[0];
  password_generation::LogPasswordGenerationEvent(
      password_generation::GENERATION_AVAILABLE);
}

bool PasswordGenerationAgent::FocusedNodeHasChanged(
    const blink::WebNode& node) {
  if (!generation_element_.isNull())
    generation_element_.setShouldRevealPassword(false);

  if (node.isNull() || !node.isElementNode())
    return false;

  const blink::WebElement web_element = node.toConst<blink::WebElement>();
  if (!web_element.document().frame())
    return false;

  const blink::WebInputElement* element = toWebInputElement(&web_element);
  if (!element || *element != generation_element_)
    return false;

  if (password_is_generated_) {
    generation_element_.setShouldRevealPassword(true);
    ShowEditingPopup();
    return true;
  }

  // Only trigger if the password field is empty.
  if (!element->isReadOnly() &&
      element->isEnabled() &&
      element->value().isEmpty()) {
    ShowGenerationPopup();
    return true;
  }

  return false;
}

bool PasswordGenerationAgent::TextDidChangeInTextField(
    const blink::WebInputElement& element) {
  if (element != generation_element_)
    return false;

  if (element.value().isEmpty()) {
    if (password_is_generated_) {
      // User generated a password and then deleted it.
      password_generation::LogPasswordGenerationEvent(
          password_generation::PASSWORD_DELETED);
    }

    // Do not treat the password as generated.
    // TODO(gcasto): Set PasswordForm::type in the browser to TYPE_NORMAL.
    password_is_generated_ = false;
    generation_element_.setShouldRevealPassword(false);

    // Offer generation again.
    ShowGenerationPopup();
  } else if (!password_is_generated_) {
    // User has rejected the feature and has started typing a password.
    HidePopup();
  } else {
    password_edited_ = true;
    // Mirror edits to any confirmation password fields.
    for (std::vector<blink::WebInputElement>::iterator it =
             password_elements_.begin();
         it != password_elements_.end(); ++it) {
      it->setValue(element.value());
    }
  }

  return true;
}

void PasswordGenerationAgent::ShowGenerationPopup() {
  gfx::RectF bounding_box_scaled =
      GetScaledBoundingBox(render_view_->GetWebView()->pageScaleFactor(),
                           &generation_element_);

  Send(new AutofillHostMsg_ShowPasswordGenerationPopup(
      routing_id(),
      bounding_box_scaled,
      generation_element_.maxLength(),
      *possible_account_creation_form_));

  password_generation::LogPasswordGenerationEvent(
      password_generation::GENERATION_POPUP_SHOWN);
}

void PasswordGenerationAgent::ShowEditingPopup() {
  gfx::RectF bounding_box_scaled =
      GetScaledBoundingBox(render_view_->GetWebView()->pageScaleFactor(),
                           &generation_element_);

  Send(new AutofillHostMsg_ShowPasswordEditingPopup(
      routing_id(),
      bounding_box_scaled,
      *possible_account_creation_form_));

  password_generation::LogPasswordGenerationEvent(
      password_generation::EDITING_POPUP_SHOWN);
}

void PasswordGenerationAgent::HidePopup() {
  Send(new AutofillHostMsg_HidePasswordGenerationPopup(routing_id()));
}

}  // namespace autofill
