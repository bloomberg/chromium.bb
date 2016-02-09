// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_GENERATION_AGENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_GENERATION_AGENT_H_

#include <stddef.h>

#include <map>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "url/gurl.h"

namespace blink {
class WebDocument;
}

namespace autofill {

struct FormData;
struct PasswordForm;
struct PasswordFormGenerationData;
class PasswordAutofillAgent;

// This class is responsible for controlling communication for password
// generation between the browser (which shows the popup and generates
// passwords) and WebKit (shows the generation icon in the password field).
class PasswordGenerationAgent : public content::RenderFrameObserver {
 public:
  PasswordGenerationAgent(content::RenderFrame* render_frame,
                          PasswordAutofillAgent* password_agent);
  ~PasswordGenerationAgent() override;

  // Returns true if the field being changed is one where a generated password
  // is being offered. Updates the state of the popup if necessary.
  bool TextDidChangeInTextField(const blink::WebInputElement& element);

  // Returns true if the newly focused node caused the generation UI to show.
  bool FocusedNodeHasChanged(const blink::WebNode& node);

  // Called when new form controls are inserted.
  void OnDynamicFormsSeen();

  // The length that a password can be before the UI is hidden.
  static const size_t kMaximumOfferSize = 5;

 protected:
  // Returns true if the document for |render_frame()| is one that we should
  // consider analyzing. Virtual so that it can be overriden during testing.
  virtual bool ShouldAnalyzeDocument() const;

  // RenderViewObserver:
  bool OnMessageReceived(const IPC::Message& message) override;

  // Use to force enable during testing.
  void set_enabled(bool enabled) { enabled_ = enabled; }

 private:
  struct AccountCreationFormData {
    linked_ptr<PasswordForm> form;
    std::vector<blink::WebInputElement> password_elements;

    AccountCreationFormData(
        linked_ptr<PasswordForm> form,
        std::vector<blink::WebInputElement> password_elements);
    ~AccountCreationFormData();
  };

  typedef std::vector<AccountCreationFormData> AccountCreationFormDataList;

  // RenderFrameObserver:
  void DidFinishDocumentLoad() override;

  // Message handlers.
  void OnFormNotBlacklisted(const PasswordForm& form);
  void OnPasswordAccepted(const base::string16& password);
  void OnFormsEligibleForGenerationFound(
      const std::vector<autofill::PasswordFormGenerationData>& forms);

  // Helper function that will try and populate |password_elements_| and
  // |possible_account_creation_form_|.
  void FindPossibleGenerationForm();

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

  // Sets |generation_element_| to the focused password field and shows a
  // generation popup at this field.
  void OnGeneratePassword();

  // Stores forms that are candidates for account creation.
  AccountCreationFormDataList possible_account_creation_forms_;

  // Stores the origins of the password forms confirmed not to be blacklisted
  // by the browser. A form can be blacklisted if a user chooses "never save
  // passwords for this site".
  std::vector<GURL> not_blacklisted_password_form_origins_;

  // Stores each password form for which the Autofill server classifies one of
  // the form's fields as an ACCOUNT_CREATION_PASSWORD or NEW_PASSWORD. These
  // forms will not be sent if the feature is disabled.
  std::vector<autofill::PasswordFormGenerationData> generation_enabled_forms_;

  // Data for form which generation is allowed on.
  scoped_ptr<AccountCreationFormData> generation_form_data_;

  // Element where we want to trigger password generation UI.
  blink::WebInputElement generation_element_;

  // If the password field at |generation_element_| contains a generated
  // password.
  bool password_is_generated_;

  // True if a password was generated and the user edited it. Used for UMA
  // stats.
  bool password_edited_;

  // True if the generation popup was shown during this navigation. Used to
  // track UMA stats per page visit rather than per display, since the former
  // is more interesting.
  bool generation_popup_shown_;

  // True if the editing popup was shown during this navigation. Used to track
  // UMA stats per page rather than per display, since the former is more
  // interesting.
  bool editing_popup_shown_;

  // If this feature is enabled. Controlled by Finch.
  bool enabled_;

  // Unowned pointer. Used to notify PassowrdAutofillAgent when values
  // in password fields are updated.
  PasswordAutofillAgent* password_agent_;

  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationAgent);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_GENERATION_AGENT_H_
