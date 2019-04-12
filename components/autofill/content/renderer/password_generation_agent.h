// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_GENERATION_AGENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_GENERATION_AGENT_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "components/autofill/content/common/autofill_agent.mojom.h"
#include "components/autofill/content/common/autofill_driver.mojom.h"
#include "components/autofill/content/renderer/renderer_save_password_progress_logger.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "url/gurl.h"

namespace autofill {

struct PasswordForm;
struct PasswordFormGenerationData;
class PasswordAutofillAgent;

// This class is responsible for controlling communication for password
// generation between the browser (which shows the popup and generates
// passwords) and WebKit (shows the generation icon in the password field).
class PasswordGenerationAgent : public content::RenderFrameObserver,
                                public mojom::PasswordGenerationAgent {
 public:
  // Maximum number of characters typed by user while the generation is still
  // offered. When the (kMaximumCharsForGenerationOffer + 1)-th character is
  // typed, the generation becomes unavailable.
  static const size_t kMaximumCharsForGenerationOffer = 5;

  // User can edit the generated password. If the length falls below this value,
  // the password is no longer considered generated.
  static const size_t kMinimumLengthForEditedPassword = 4;

  PasswordGenerationAgent(content::RenderFrame* render_frame,
                          PasswordAutofillAgent* password_agent,
                          blink::AssociatedInterfaceRegistry* registry);
  ~PasswordGenerationAgent() override;

  void BindRequest(mojom::PasswordGenerationAgentAssociatedRequest request);

  // mojom::PasswordGenerationAgent:
  void FormNotBlacklisted(const PasswordForm& form) override;
  void GeneratedPasswordAccepted(const base::string16& password) override;
  void FoundFormsEligibleForGeneration(
      const std::vector<PasswordFormGenerationData>& forms) override;
  void FoundFormEligibleForGeneration(
      const NewPasswordFormGenerationData& form) override;
  // Sets |generation_element_| to the focused password field and responds back
  // if the generation was triggered successfully.
  void UserTriggeredGeneratePassword(
      UserTriggeredGeneratePasswordCallback callback) override;

  // Returns true if the field being changed is one where a generated password
  // is being offered. Updates the state of the popup if necessary.
  bool TextDidChangeInTextField(const blink::WebInputElement& element);

  // Returns true if the newly focused node caused the generation UI to show.
  bool FocusedNodeHasChanged(const blink::WebNode& node);

  // Event forwarded by AutofillAgent from WebAutofillClient, informing that
  // the text field editing has ended, which means that the field is not
  // focused anymore. This is required for Android, where moving focus
  // to a non-focusable element doesn't result in |FocusedNodeHasChanged|
  // being called.
  void DidEndTextFieldEditing(const blink::WebInputElement& element);

  // Called when new form controls are inserted.
  void OnDynamicFormsSeen();

  // Called right before PasswordAutofillAgent filled |password_element|.
  void OnFieldAutofilled(const blink::WebInputElement& password_element);

  // Returns true iff the currently handled 'blur' event is fake and should be
  // ignored.
  bool ShouldIgnoreBlur() const;

#if defined(UNIT_TEST)
  // This method requests the autofill::mojom::PasswordManagerClient which binds
  // requests the binding if it wasn't bound yet.
  void RequestPasswordManagerClientForTesting() {
    GetPasswordGenerationDriver();
  }
#endif

 protected:
  // Returns true if the document for |render_frame()| is one that we should
  // consider analyzing. Virtual so that it can be overriden during testing.
  virtual bool ShouldAnalyzeDocument();

  // Use to force enable during testing.
  void set_enabled(bool enabled) { enabled_ = enabled; }

 private:
  // Contains information about a form for which generation is possible.
  struct AccountCreationFormData;
  // Contains information about generation status for an element for the
  // lifetime of the possible interaction.
  struct GenerationItemInfo;

  typedef std::vector<AccountCreationFormData> AccountCreationFormDataList;

  // RenderFrameObserver:
  void DidCommitProvisionalLoad(bool is_same_document_navigation,
                                ui::PageTransition transition) override;
  void DidChangeScrollOffset() override;
  void DidFinishDocumentLoad() override;
  void DidFinishLoad() override;
  void OnDestruct() override;

  const mojom::PasswordManagerDriverAssociatedPtr& GetPasswordManagerDriver();

  const mojom::PasswordGenerationDriverAssociatedPtr&
  GetPasswordGenerationDriver();

  // Helper function that will try and populate |password_elements_| and
  // |possible_account_creation_form_|.
  void FindPossibleGenerationForm();

  // Helper function to decide if |possible_account_creation_forms_| contains
  // password fields for an account creation form. Sets
  // |automatic_generation_element_| to the field that we want to trigger the
  // generation UI on.
  void DetermineGenerationElement();

  // Helper function which takes care of the form processing and collecting the
  // information which is required to show the generation popup. Returns true if
  // all required information is collected.
  bool SetUpUserTriggeredGeneration();

  // This is called whenever automatic generation could be offered.
  // If manual generation was already requested, automatic generation will
  // not be offered.
  void MaybeOfferAutomaticGeneration();

  // Signals the browser that it should offer automatic password generation
  // as a result of the user focusing a password field eligible for generation.
  void AutomaticGenerationAvailable();

  // Show UI for editing a generated password at |generation_element_|.
  void ShowEditingPopup();

  // Signals the browser that generation was rejected. This happens when the
  // user types more characters than the maximum offer size into the password
  // field. Upon receiving this message, the browser can choose to hide the
  // generation UI or not, depending on the platform.
  void GenerationRejectedByTyping();

  // Stops treating a password as generated.
  void PasswordNoLongerGenerated();

  // Creates |current_generation_item_| for |element| if |element| is a
  // generation enabled element. If |current_generation_item_| is already
  // created for |element| it is not recreated.
  void MaybeCreateCurrentGenerationItem(
      blink::WebInputElement element,
      uint32_t confirmation_password_renderer_id);

  // Runs HTML parsing based classifier and saves its outcome to proto.
  // TODO(crbug.com/621442): Remove client-side form classifier when server-side
  // classifier is ready.
  void RunFormClassifierAndSaveVote(const blink::WebFormElement& web_form,
                                    const PasswordForm& form);

  void LogMessage(autofill::SavePasswordProgressLogger::StringID message_id);
  void LogBoolean(autofill::SavePasswordProgressLogger::StringID message_id,
                  bool truth_value);
  void LogNumber(autofill::SavePasswordProgressLogger::StringID message_id,
                 int number);

  // Creates a password form to presave a generated password. It copies behavior
  // of CreatePasswordFormFromWebForm/FromUnownedInputElements, but takes
  // |password_value| from |generation_element_| and empties |username_value|.
  // If a form creating is failed, returns an empty unique_ptr.
  std::unique_ptr<PasswordForm> CreatePasswordFormToPresave();

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

  // Data for form which automatic generation is allowed on.
  std::unique_ptr<AccountCreationFormData> automatic_generation_form_data_;

  // Element where we want to trigger automatic password generation UI on.
  blink::WebInputElement automatic_generation_element_;

  // Contains the current element where generation is offered at the moment. It
  // can be either automatic or manual password generation.
  std::unique_ptr<GenerationItemInfo> current_generation_item_;

  // Password element that had focus last. Since Javascript could change focused
  // element after the user triggered a generation request, it is better to save
  // the last focused password element.
  blink::WebInputElement last_focused_password_element_;

  // Contains correspondence between generaiton enabled element and data for
  // generation.
  std::map<uint32_t, NewPasswordFormGenerationData> generation_enabled_fields_;

  // If this feature is enabled. Controlled by Finch.
  bool enabled_;

  // True iff the generation element should be marked with special HTML
  // attribute (only for experimental purposes).
  const bool mark_generation_element_;

  // Unowned pointer. Used to notify PassowrdAutofillAgent when values
  // in password fields are updated.
  PasswordAutofillAgent* password_agent_;

  mojom::PasswordGenerationDriverAssociatedPtr password_generation_client_;

  mojo::AssociatedBinding<mojom::PasswordGenerationAgent> binding_;

  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationAgent);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_GENERATION_AGENT_H_
