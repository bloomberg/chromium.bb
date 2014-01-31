// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/password_generation_agent.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "components/autofill/content/common/autofill_messages.h"
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
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
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
      render_view_(render_view) {
  render_view_->GetWebView()->setPasswordGeneratorClient(this);
}
PasswordGenerationAgent::~PasswordGenerationAgent() {}

void PasswordGenerationAgent::DidFinishDocumentLoad(blink::WebFrame* frame) {
  // In every navigation, the IPC message sent by the password autofill manager
  // to query whether the current form is blacklisted or not happens when the
  // document load finishes, so we need to clear previous states here before we
  // hear back from the browser. We only clear this state on main frame load
  // as we don't want subframe loads to clear state that we have recieved from
  // the main frame. Note that we assume there is only one account creation
  // form, but there could be multiple password forms in each frame.
  //
  // TODO(zysxqn): Add stat when local heuristic fires but we don't show the
  // password generation icon.
  if (!frame->parent()) {
    not_blacklisted_password_form_origins_.clear();
    generation_enabled_forms_.clear();
    possible_account_creation_form_.reset(new PasswordForm());
    passwords_.clear();
  }
}

void PasswordGenerationAgent::DidFinishLoad(blink::WebFrame* frame) {
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
      passwords_ = passwords;
      possible_account_creation_form_.swap(password_form);
      MaybeShowIcon();
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

void PasswordGenerationAgent::openPasswordGenerator(
    blink::WebInputElement& element) {
  blink::WebElement button(element.passwordGeneratorButtonElement());
  gfx::Rect rect(button.boundsInViewportSpace());
  scoped_ptr<PasswordForm> password_form(
      CreatePasswordForm(element.form()));
  // We should not have shown the icon we can't create a valid PasswordForm.
  DCHECK(password_form.get());

  Send(new AutofillHostMsg_ShowPasswordGenerationPopup(routing_id(),
                                                       rect,
                                                       element.maxLength(),
                                                       *password_form));
  password_generation::LogPasswordGenerationEvent(
      password_generation::BUBBLE_SHOWN);
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
  MaybeShowIcon();
}

void PasswordGenerationAgent::OnPasswordAccepted(
    const base::string16& password) {
  for (std::vector<blink::WebInputElement>::iterator it = passwords_.begin();
       it != passwords_.end(); ++it) {
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
  MaybeShowIcon();
}

void PasswordGenerationAgent::MaybeShowIcon() {
  // Make sure local heuristics have identified a possible account creation
  // form.
  if (!possible_account_creation_form_.get() || passwords_.empty()) {
    DVLOG(2) << "Local hueristics have not detected a possible account "
             << "creation form";
    return;
  }

  // Verify that it's not blacklisted.
  if (not_blacklisted_password_form_origins_.empty() ||
      !ContainsURL(not_blacklisted_password_form_origins_,
                   possible_account_creation_form_->origin)) {
    DVLOG(2) << "Have not recieved confirmation that password form isn't "
             << "blacklisted";
    return;
  }

  // Ensure that we get a ping from Autofill saying that this form is used for
  // account creation. Note that this message will not be set if this feature
  // is not enabled. If kNoAutofillNecessaryForPasswordGeneration is set,
  // skip this check. This switch should only be used in testing environments.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNoAutofillNecessaryForPasswordGeneration) &&
      (generation_enabled_forms_.empty() ||
       !ContainsForm(generation_enabled_forms_,
                     *possible_account_creation_form_))) {
    DVLOG(2) << "Have not recieved confirmation from Autofill that form is used"
             << " for account creation";
    return;
  }

  passwords_[0].passwordGeneratorButtonElement().setAttribute("style",
                                                              "display:block");
  password_generation::LogPasswordGenerationEvent(
      password_generation::ICON_SHOWN);
}

}  // namespace autofill
