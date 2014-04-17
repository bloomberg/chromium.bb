// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_GENERATION_AGENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_GENERATION_AGENT_H_

#include <map>
#include <utility>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "url/gurl.h"

namespace blink {
class WebCString;
class WebDocument;
}

namespace autofill {

struct FormData;
struct PasswordForm;

// This class is responsible for controlling communication for password
// generation between the browser (which shows the popup and generates
// passwords) and WebKit (shows the generation icon in the password field).
class PasswordGenerationAgent : public content::RenderViewObserver {
 public:
  explicit PasswordGenerationAgent(content::RenderView* render_view);
  virtual ~PasswordGenerationAgent();

  // Returns true if the field being changed is one where a generated password
  // is being offered. Updates the state of the popup if necessary.
  bool TextDidChangeInTextField(const blink::WebInputElement& element);

  // Returns true if the newly focused node caused the generation UI to show.
  bool FocusedNodeHasChanged(const blink::WebNode& node);

 protected:
  // Returns true if this document is one that we should consider analyzing.
  // Virtual so that it can be overriden during testing.
  virtual bool ShouldAnalyzeDocument(const blink::WebDocument& document) const;

  // RenderViewObserver:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Use to force enable during testing.
  void set_enabled(bool enabled) { enabled_ = enabled; }

 private:
  // RenderViewObserver:
  virtual void DidFinishDocumentLoad(blink::WebLocalFrame* frame) OVERRIDE;
  virtual void DidFinishLoad(blink::WebLocalFrame* frame) OVERRIDE;

  // Message handlers.
  void OnFormNotBlacklisted(const PasswordForm& form);
  void OnPasswordAccepted(const base::string16& password);
  void OnAccountCreationFormsDetected(
      const std::vector<autofill::FormData>& forms);

  // Helper function to decide if |passwords_| contains password fields for
  // an account creation form. Sets |generation_element_| to the field that
  // we want to trigger the generation UI on.
  void DetermineGenerationElement();

  // Show password generation UI anchored at |generation_element_|.
  void ShowGenerationPopup();

  // Show UI for editing a generated password at |generation_element_|.
  void ShowEditingPopup();

  // Hides a password generation popup if one exists.
  void HidePopup();

  content::RenderView* render_view_;

  // Stores the origin of the account creation form we detected.
  scoped_ptr<PasswordForm> possible_account_creation_form_;

  // Stores the origins of the password forms confirmed not to be blacklisted
  // by the browser. A form can be blacklisted if a user chooses "never save
  // passwords for this site".
  std::vector<GURL> not_blacklisted_password_form_origins_;

  // Stores each password form for which the Autofill server classifies one of
  // the form's fields as an ACCOUNT_CREATION_PASSWORD. These forms will
  // not be sent if the feature is disabled.
  std::vector<autofill::FormData> generation_enabled_forms_;

  // Password elements that may be part of an account creation form.
  std::vector<blink::WebInputElement> password_elements_;

  // Element where we want to trigger password generation UI.
  blink::WebInputElement generation_element_;

  // If the password field at |generation_element_| contains a generated
  // password.
  bool password_is_generated_;

  // True if a password was generated and the user edited it. Used for UMA
  // stats.
  bool password_edited_;

  // If this feature is enabled. Controlled by Finch.
  bool enabled_;

  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationAgent);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_GENERATION_AGENT_H_
