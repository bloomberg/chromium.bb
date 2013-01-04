// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/autofill/password_generation_manager.h"

#include "base/logging.h"
#include "chrome/common/autofill_messages.h"
#include "chrome/common/password_generation_util.h"
#include "content/public/renderer/password_form_conversion_utils.h"
#include "content/public/renderer/render_view.h"
#include "google_apis/gaia/gaia_urls.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/gfx/rect.h"

namespace autofill {

namespace {

// Returns true if we think that this form is for account creation. |passwords|
// is filled with the password field(s) in the form.
bool GetAccountCreationPasswordFields(
    const WebKit::WebFormElement& form,
    std::vector<WebKit::WebInputElement>* passwords) {
  // Grab all of the passwords for the form.
  WebKit::WebVector<WebKit::WebFormControlElement> control_elements;
  form.getFormControlElements(control_elements);

  size_t num_input_elements = 0;
  for (size_t i = 0; i < control_elements.size(); i++) {
    WebKit::WebInputElement* input_element =
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

}  // namespace

PasswordGenerationManager::PasswordGenerationManager(
    content::RenderView* render_view)
    : content::RenderViewObserver(render_view),
      render_view_(render_view),
      enabled_(false) {
  render_view_->GetWebView()->addTextFieldDecoratorClient(this);
}
PasswordGenerationManager::~PasswordGenerationManager() {}

void PasswordGenerationManager::DidFinishDocumentLoad(WebKit::WebFrame* frame) {
  // In every navigation, the IPC message sent by the password autofill manager
  // to query whether the current form is blacklisted or not happens when the
  // document load finishes, so we need to clear previous states here before we
  // hear back from the browser. We only clear this state on main frame load
  // as we don't want subframe loads to clear state that we have recieved from
  // the main frame. Note that we assume there is only one account creation
  // form, but there could be multiple password forms in each frame.
  if (!frame->parent()) {
    not_blacklisted_password_form_origins_.clear();
    // Initialize to an empty and invalid GURL.
    account_creation_form_origin_ = GURL();
    passwords_.clear();
  }
}

void PasswordGenerationManager::DidFinishLoad(WebKit::WebFrame* frame) {
  // We don't want to generate passwords if the browser won't store or sync
  // them.
  if (!enabled_)
    return;

  if (!ShouldAnalyzeDocument(frame->document()))
    return;

  WebKit::WebVector<WebKit::WebFormElement> forms;
  frame->document().forms(forms);
  for (size_t i = 0; i < forms.size(); ++i) {
    if (forms[i].isNull())
      continue;

    // If we can't get a valid PasswordForm, we skip this form because the
    // the password won't get saved even if we generate it.
    scoped_ptr<content::PasswordForm> password_form(
        content::CreatePasswordForm(forms[i]));
    if (!password_form.get()) {
      DVLOG(2) << "Skipping form as it would not be saved";
      continue;
    }

    // Do not generate password for GAIA since it is used to retrieve the
    // generated paswords.
    GURL realm(password_form->signon_realm);
    if (realm == GURL(GaiaUrls::GetInstance()->gaia_login_form_realm()))
      continue;

    std::vector<WebKit::WebInputElement> passwords;
    if (GetAccountCreationPasswordFields(forms[i], &passwords)) {
      DVLOG(2) << "Account creation form detected";
      password_generation::LogPasswordGenerationEvent(
          password_generation::SIGN_UP_DETECTED);
      passwords_ = passwords;
      account_creation_form_origin_ = password_form->origin;
      MaybeShowIcon();
      // We assume that there is only one account creation field per URL.
      return;
    }
  }
  password_generation::LogPasswordGenerationEvent(
      password_generation::NO_SIGN_UP_DETECTED);
}

bool PasswordGenerationManager::ShouldAnalyzeDocument(
    const WebKit::WebDocument& document) const {
  // Make sure that this security origin is allowed to use password manager.
  // Generating a password that can't be saved is a bad idea.
  WebKit::WebSecurityOrigin origin = document.securityOrigin();
  if (!origin.canAccessPasswordManager()) {
    DVLOG(1) << "No PasswordManager access";
    return false;
  }

  return true;
}

bool PasswordGenerationManager::shouldAddDecorationTo(
    const WebKit::WebInputElement& element) {
  return element.isPasswordField();
}

bool PasswordGenerationManager::visibleByDefault() {
  return false;
}

WebKit::WebCString PasswordGenerationManager::imageNameForNormalState() {
  return WebKit::WebCString("generatePassword");
}

WebKit::WebCString PasswordGenerationManager::imageNameForDisabledState() {
  return imageNameForNormalState();
}

WebKit::WebCString PasswordGenerationManager::imageNameForReadOnlyState() {
  return imageNameForNormalState();
}

WebKit::WebCString PasswordGenerationManager::imageNameForHoverState() {
  return WebKit::WebCString("generatePasswordHover");
}

void PasswordGenerationManager::handleClick(WebKit::WebInputElement& element) {
  gfx::Rect rect(element.decorationElementFor(this).boundsInViewportSpace());
  scoped_ptr<content::PasswordForm> password_form(
      content::CreatePasswordForm(element.form()));
  // We should not have shown the icon we can't create a valid PasswordForm.
  DCHECK(password_form.get());

  Send(new AutofillHostMsg_ShowPasswordGenerationPopup(routing_id(),
                                                       rect,
                                                       element.maxLength(),
                                                       *password_form));
  password_generation::LogPasswordGenerationEvent(
      password_generation::BUBBLE_SHOWN);
}

void PasswordGenerationManager::willDetach(
    const WebKit::WebInputElement& element) {
  // No implementation
}

bool PasswordGenerationManager::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PasswordGenerationManager, message)
    IPC_MESSAGE_HANDLER(AutofillMsg_FormNotBlacklisted,
                        OnFormNotBlacklisted)
    IPC_MESSAGE_HANDLER(AutofillMsg_GeneratedPasswordAccepted,
                        OnPasswordAccepted)
    IPC_MESSAGE_HANDLER(AutofillMsg_PasswordGenerationEnabled,
                        OnPasswordGenerationEnabled)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PasswordGenerationManager::OnFormNotBlacklisted(
    const content::PasswordForm& form) {
  not_blacklisted_password_form_origins_.push_back(form.origin);
  MaybeShowIcon();
}

void PasswordGenerationManager::OnPasswordAccepted(const string16& password) {
  for (std::vector<WebKit::WebInputElement>::iterator it = passwords_.begin();
       it != passwords_.end(); ++it) {
    it->setValue(password);
    it->setAutofilled(true);
    // Advance focus to the next input field. We assume password fields in
    // an account creation form are always adjacent.
    render_view_->GetWebView()->advanceFocus(false);
  }
}

void PasswordGenerationManager::OnPasswordGenerationEnabled(bool enabled) {
  enabled_ = enabled;
}

void PasswordGenerationManager::MaybeShowIcon() {
  // We should show the password generation icon only when we have detected
  // account creation form and we have confirmed from browser that this form
  // is not blacklisted by the users.
  if (!account_creation_form_origin_.is_valid() ||
      passwords_.empty() ||
      not_blacklisted_password_form_origins_.empty()) {
    return;
  }

  for (std::vector<GURL>::iterator it =
           not_blacklisted_password_form_origins_.begin();
       it != not_blacklisted_password_form_origins_.end(); ++it) {
    if (*it == account_creation_form_origin_) {
      passwords_[0].decorationElementFor(this).setAttribute("style",
                                                            "display:block");
      password_generation::LogPasswordGenerationEvent(
          password_generation::ICON_SHOWN);
      return;
    }
  }
}

}  // namespace autofill
