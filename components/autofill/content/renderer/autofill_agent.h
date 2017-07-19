// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_AGENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_AGENT_H_

#include <set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/autofill/content/common/autofill_agent.mojom.h"
#include "components/autofill/content/common/autofill_driver.mojom.h"
#include "components/autofill/content/renderer/form_cache.h"
#include "components/autofill/content/renderer/page_click_listener.h"
#include "components/autofill/content/renderer/page_click_tracker.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/WebKit/public/web/WebAutofillClient.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebInputElement.h"

namespace blink {
class WebNode;
class WebView;
}

namespace autofill {

struct FormData;
struct FormFieldData;
struct PasswordFormFillData;
class PasswordAutofillAgent;
class PasswordGenerationAgent;

// AutofillAgent deals with Autofill related communications between WebKit and
// the browser.  There is one AutofillAgent per RenderFrame.
// Note that Autofill encompasses:
// - single text field suggestions, that we usually refer to as Autocomplete,
// - password form fill, refered to as Password Autofill, and
// - entire form fill based on one field entry, referred to as Form Autofill.

class AutofillAgent : public content::RenderFrameObserver,
                      public PageClickListener,
                      public blink::WebAutofillClient,
                      public mojom::AutofillAgent {
 public:
  // PasswordAutofillAgent is guaranteed to outlive AutofillAgent.
  // PasswordGenerationAgent may be NULL. If it is not, then it is also
  // guaranteed to outlive AutofillAgent.
  AutofillAgent(content::RenderFrame* render_frame,
                PasswordAutofillAgent* password_autofill_manager,
                PasswordGenerationAgent* password_generation_agent);
  ~AutofillAgent() override;

  void BindRequest(mojom::AutofillAgentRequest request);

  const mojom::AutofillDriverPtr& GetAutofillDriver();

  const mojom::PasswordManagerDriverPtr& GetPasswordManagerDriver();

  // mojom::AutofillAgent:
  void FillForm(int32_t id, const FormData& form) override;
  void PreviewForm(int32_t id, const FormData& form) override;
  void FieldTypePredictionsAvailable(
      const std::vector<FormDataPredictions>& forms) override;
  void ClearForm() override;
  void ClearPreviewedForm() override;
  void FillFieldWithValue(const base::string16& value) override;
  void PreviewFieldWithValue(const base::string16& value) override;
  void AcceptDataListSuggestion(const base::string16& value) override;
  void FillPasswordSuggestion(const base::string16& username,
                              const base::string16& password) override;
  void PreviewPasswordSuggestion(const base::string16& username,
                                 const base::string16& password) override;
  void ShowInitialPasswordAccountSuggestions(
      int32_t key,
      const PasswordFormFillData& form_data) override;
  void SetUserGestureRequired(bool required) override;
  void SetSecureContextRequired(bool required) override;

  void ShowNotSecureWarning(const blink::WebInputElement& element);

  void set_page_click_tracker_for_testing(
      std::unique_ptr<PageClickTracker> page_click_tracker) {
    page_click_tracker_ = std::move(page_click_tracker);
  }

 protected:
  // blink::WebAutofillClient:
  void DidAssociateFormControlsDynamically() override;

 private:
  // Functor used as a simplified comparison function for FormData. Only
  // compares forms at a high level (notably name, origin, action).
  struct FormDataCompare {
    bool operator()(const FormData& lhs, const FormData& rhs) const;
  };

  // Flags passed to ShowSuggestions.
  struct ShowSuggestionsOptions {
    // All fields are default initialized to false.
    ShowSuggestionsOptions();

    // Specifies that suggestions should be shown when |element| contains no
    // text.
    bool autofill_on_empty_values;

    // Specifies that suggestions should be shown when the caret is not
    // after the last character in the element.
    bool requires_caret_at_end;

    // Specifies that all of <datalist> suggestions and no autofill suggestions
    // are shown. |autofill_on_empty_values| and |requires_caret_at_end| are
    // ignored if |datalist_only| is true.
    bool datalist_only;

    // Specifies that all autofill suggestions should be shown and none should
    // be elided because of the current value of |element| (relevant for inline
    // autocomplete).
    bool show_full_suggestion_list;

    // Specifies that only show a suggestions box if |element| is part of a
    // password form, otherwise show no suggestions.
    bool show_password_suggestions_only;
  };

  // content::RenderFrameObserver:
  void DidCommitProvisionalLoad(bool is_new_navigation,
                                bool is_same_document_navigation) override;
  void DidFinishDocumentLoad() override;
  void WillSendSubmitEvent(const blink::WebFormElement& form) override;
  void WillSubmitForm(const blink::WebFormElement& form) override;
  void DidChangeScrollOffset() override;
  void FocusedNodeChanged(const blink::WebNode& node) override;
  void OnDestruct() override;

  // Fires IPC messages for a given form submission. Will always fire
  // AutofillHostMsg_WillSubmitForm,  and will also fire
  // AutofillHostMsg_FormSubmitted if |form_submitted| is true. Respects
  // submitted_forms_ contents to ensure no duplicate submissions of
  // AutofillHostMsg_WillSubmitForm.
  void FireHostSubmitEvents(const blink::WebFormElement& form,
                            bool form_submitted);
  void FireHostSubmitEvents(const FormData& form_data, bool form_submitted);

  // Shuts the AutofillAgent down on RenderFrame deletion. Safe to call multiple
  // times.
  void Shutdown();

  // PageClickListener:
  void FormControlElementClicked(const blink::WebFormControlElement& element,
                                 bool was_focused) override;

  // blink::WebAutofillClient:
  void TextFieldDidEndEditing(const blink::WebInputElement& element) override;
  void TextFieldDidChange(const blink::WebFormControlElement& element) override;
  void TextFieldDidReceiveKeyDown(
      const blink::WebInputElement& element,
      const blink::WebKeyboardEvent& event) override;
  void OpenTextDataListChooser(const blink::WebInputElement& element) override;
  void DataListOptionsChanged(const blink::WebInputElement& element) override;
  void UserGestureObserved() override;
  void AjaxSucceeded() override;
  void DidCompleteFocusChangeInFrame() override;
  void DidReceiveLeftMouseDownOrGestureTapInNode(
      const blink::WebNode& node) override;

  // Called when a same-document navigation is detected.
  void OnSameDocumentNavigationCompleted();
  // Helper method which collects unowned elements (i.e., those not inside a
  // form tag) and writes them into |output|. Returns true if the process is
  // successful, and all conditions for firing events are true.
  bool CollectFormlessElements(FormData* output);
  FRIEND_TEST_ALL_PREFIXES(FormAutocompleteTest, CollectFormlessElements);

  // Called in a posted task by textFieldDidChange() to work-around a WebKit bug
  // http://bugs.webkit.org/show_bug.cgi?id=16976
  void TextFieldDidChangeImpl(const blink::WebFormControlElement& element);

  // Shows the autofill suggestions for |element|. This call is asynchronous
  // and may or may not lead to the showing of a suggestion popup (no popup is
  // shown if there are no available suggestions).
  void ShowSuggestions(const blink::WebFormControlElement& element,
                       const ShowSuggestionsOptions& options);

  // Queries the browser for Autocomplete and Autofill suggestions for the given
  // |element|.
  void QueryAutofillSuggestions(const blink::WebFormControlElement& element);

  // Sets the element value to reflect the selected |suggested_value|.
  void DoAcceptDataListSuggestion(const base::string16& suggested_value);

  // Fills |form| and |field| with the FormData and FormField corresponding to
  // |node|. Returns true if the data was found; and false otherwise.
  bool FindFormAndFieldForNode(
      const blink::WebNode& node,
      FormData* form,
      FormFieldData* field) WARN_UNUSED_RESULT;

  // Set |node| to display the given |value|.
  void DoFillFieldWithValue(const base::string16& value,
                            blink::WebInputElement* node);

  // Set |node| to display the given |value| as a preview.  The preview is
  // visible on screen to the user, but not visible to the page via the DOM or
  // JavaScript.
  void DoPreviewFieldWithValue(const base::string16& value,
                               blink::WebInputElement* node);

  // Notifies browser of new fillable forms in |render_frame|.
  void ProcessForms();

  // Sends a message to the browser that the form is about to be submitted,
  // only if the particular message has not been previously submitted for the
  // form in the current frame.
  // Additionally, depending on |send_submitted_event| a message is sent to the
  // browser that the form was submitted.
  void SendFormEvents(const blink::WebFormElement& form,
                      bool send_submitted_event);

  // Hides any currently showing Autofill popup.
  void HidePopup();

  // Returns true if the text field change is due to a user gesture. Can be
  // overriden in tests.
  virtual bool IsUserGesture() const;

  // Formerly cached forms for all frames, now only caches forms for the current
  // frame.
  FormCache form_cache_;

  // Keeps track of the forms for which a "will submit" message has been sent in
  // this frame's current load. We use a simplified comparison function.
  std::set<FormData, FormDataCompare> submitted_forms_;

  PasswordAutofillAgent* password_autofill_agent_;  // Weak reference.
  PasswordGenerationAgent* password_generation_agent_;  // Weak reference.

  // The ID of the last request sent for form field Autofill.  Used to ignore
  // out of date responses.
  int autofill_query_id_;

  // The element corresponding to the last request sent for form field Autofill.
  blink::WebFormControlElement element_;

  // The form element currently requesting an interactive autocomplete.
  blink::WebFormElement in_flight_request_form_;

  // Last form which was interacted with by the user.
  blink::WebFormElement last_interacted_form_;

  // When dealing with forms that don't use a <form> tag, we keep track of the
  // elements the user has modified so we can determine when submission occurs.
  std::set<blink::WebFormControlElement> formless_elements_user_edited_;

  // Was the query node autofilled prior to previewing the form?
  bool was_query_node_autofilled_;

  // Whether or not to ignore text changes.  Useful for when we're committing
  // a composition when we are defocusing the WebView and we don't want to
  // trigger an autofill popup to show.
  bool ignore_text_changes_;

  // Whether the Autofill popup is possibly visible.  This is tracked as a
  // performance improvement, so that the IPC channel isn't flooded with
  // messages to close the Autofill popup when it can't possibly be showing.
  bool is_popup_possibly_visible_;

  // If the generation popup is possibly visible. This is tracked to prevent
  // generation UI from displaying at the same time as password manager UI.
  // This is needed because generation is shown on field focus vs. field click
  // for the password manager. TODO(gcasto): Have both UIs show on focus.
  bool is_generation_popup_possibly_visible_;

  // Whether or not a user gesture is required before notification of a text
  // field change. Default to true.
  bool is_user_gesture_required_;

  // Whether or not the secure context is required to query autofill suggestion.
  // Default to false.
  bool is_secure_context_required_;

  std::unique_ptr<PageClickTracker> page_click_tracker_;

  mojo::Binding<mojom::AutofillAgent> binding_;

  mojom::AutofillDriverPtr autofill_driver_;

  base::WeakPtrFactory<AutofillAgent> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutofillAgent);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_AGENT_H_
