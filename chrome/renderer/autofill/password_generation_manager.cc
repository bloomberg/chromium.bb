// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/autofill/password_generation_manager.h"

#include "base/logging.h"
#include "chrome/common/autofill_messages.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"
#include "ui/gfx/rect.h"

namespace autofill {

PasswordGenerationManager::PasswordGenerationManager(
    content::RenderView* render_view)
    : content::RenderViewObserver(render_view),
      enabled_(false) {}
PasswordGenerationManager::~PasswordGenerationManager() {}

void PasswordGenerationManager::DidFinishDocumentLoad(WebKit::WebFrame* frame) {
  // We don't want to generate passwords if the browser won't store or sync
  // them.
  if (!enabled_)
    return;

  if (!ShouldAnalyzeFrame(*frame))
    return;

  WebKit::WebVector<WebKit::WebFormElement> forms;
  frame->document().forms(forms);
  for (size_t i = 0; i < forms.size(); ++i) {
    const WebKit::WebFormElement& web_form = forms[i];
    if (!web_form.autoComplete())
      continue;

    // Grab all of the passwords for each form.
    WebKit::WebVector<WebKit::WebFormControlElement> control_elements;
    web_form.getFormControlElements(control_elements);

    std::vector<WebKit::WebInputElement> passwords;
    for (size_t i = 0; i < control_elements.size(); i++) {
      WebKit::WebInputElement* input_element =
          toWebInputElement(&control_elements[i]);
      if (input_element && input_element->isPasswordField())
        passwords.push_back(*input_element);
    }
    // For now, just assume that if there are two password fields in the
    // form that this is meant for account creation.
    // TODO(gcasto): Determine better heauristics for this.
    if (passwords.size() == 2) {
      account_creation_elements_ = make_pair(passwords[0], passwords);
      break;
    }
  }
}

bool PasswordGenerationManager::ShouldAnalyzeFrame(
    const WebKit::WebFrame& frame) const {
  // Make sure that this security origin is allowed to use password manager.
  // Generating a password that can't be saved is a bad idea.
  WebKit::WebSecurityOrigin origin = frame.document().securityOrigin();
  if (!origin.canAccessPasswordManager()) {
    DVLOG(1) << "No PasswordManager access";
    return false;
  }

  return true;
}

void PasswordGenerationManager::FocusedNodeChanged(
    const WebKit::WebNode& node) {
  WebKit::WebInputElement input_element =
      node.toConst<WebKit::WebInputElement>();
  if (!input_element.isNull() &&
      account_creation_elements_.first == input_element) {
    gfx::Rect rect(input_element.boundsInViewportSpace());
    Send(new AutofillHostMsg_ShowPasswordGenerationPopup(routing_id(),
                                                         rect));
  }
}

bool PasswordGenerationManager::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PasswordGenerationManager, message)
    IPC_MESSAGE_HANDLER(AutofillMsg_GeneratedPasswordAccepted,
                        OnPasswordAccepted)
    IPC_MESSAGE_HANDLER(AutofillMsg_PasswordGenerationEnabled,
                        OnPasswordGenerationEnabled)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PasswordGenerationManager::OnPasswordAccepted(const string16& password) {
  for (std::vector<WebKit::WebInputElement>::iterator it =
           account_creation_elements_.second.begin();
       it != account_creation_elements_.second.end(); ++it) {
    it->setValue(password);
    it->setAutofilled(true);
  }
}

void PasswordGenerationManager::OnPasswordGenerationEnabled(bool enabled) {
  enabled_ = enabled;
}

}  // namespace autofill
