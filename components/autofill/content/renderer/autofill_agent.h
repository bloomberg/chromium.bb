// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_AGENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_AGENT_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill/content/renderer/form_cache.h"
#include "components/autofill/content/renderer/page_click_listener.h"
#include "components/autofill/core/common/forms_seen_state.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/public/web/WebAutofillClient.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebInputElement.h"

namespace blink {
class WebNode;
class WebView;
}

namespace autofill {

struct FormData;
struct FormFieldData;
struct WebElementDescriptor;
class PasswordAutofillAgent;
class PasswordGenerationAgent;

// AutofillAgent deals with Autofill related communications between WebKit and
// the browser.  There is one AutofillAgent per RenderView.
// This code was originally part of RenderView.
// Note that Autofill encompasses:
// - single text field suggestions, that we usually refer to as Autocomplete,
// - password form fill, refered to as Password Autofill, and
// - entire form fill based on one field entry, referred to as Form Autofill.

class AutofillAgent : public content::RenderViewObserver,
                      public PageClickListener,
                      public blink::WebAutofillClient {
 public:
  // PasswordAutofillAgent is guaranteed to outlive AutofillAgent.
  // PasswordGenerationAgent may be NULL. If it is not, then it is also
  // guaranteed to outlive AutofillAgent.
  AutofillAgent(content::RenderView* render_view,
                PasswordAutofillAgent* password_autofill_manager,
                PasswordGenerationAgent* password_generation_agent);
  virtual ~AutofillAgent();

 private:
  enum AutofillAction {
    AUTOFILL_NONE,     // No state set.
    AUTOFILL_FILL,     // Fill the Autofill form data.
    AUTOFILL_PREVIEW,  // Preview the Autofill form data.
  };

  // RenderView::Observer:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidFinishDocumentLoad(blink::WebFrame* frame) OVERRIDE;
  virtual void DidCommitProvisionalLoad(blink::WebFrame* frame,
                                        bool is_new_navigation) OVERRIDE;
  virtual void FrameDetached(blink::WebFrame* frame) OVERRIDE;
  virtual void WillSubmitForm(blink::WebFrame* frame,
                              const blink::WebFormElement& form) OVERRIDE;
  virtual void ZoomLevelChanged() OVERRIDE;
  virtual void DidChangeScrollOffset(blink::WebFrame* frame) OVERRIDE;
  virtual void FocusedNodeChanged(const blink::WebNode& node) OVERRIDE;
  virtual void OrientationChangeEvent(int orientation) OVERRIDE;

  // PageClickListener:
  virtual void InputElementClicked(const blink::WebInputElement& element,
                                   bool was_focused,
                                   bool is_focused) OVERRIDE;
  virtual void InputElementLostFocus() OVERRIDE;

  // blink::WebAutofillClient:
  virtual void textFieldDidEndEditing(
      const blink::WebInputElement& element) OVERRIDE;
  virtual void textFieldDidChange(
      const blink::WebInputElement& element) OVERRIDE;
  virtual void textFieldDidReceiveKeyDown(
      const blink::WebInputElement& element,
      const blink::WebKeyboardEvent& event) OVERRIDE;
  virtual void didRequestAutocomplete(
      blink::WebFrame* frame,
      const blink::WebFormElement& form) OVERRIDE;
  virtual void setIgnoreTextChanges(bool ignore) OVERRIDE;
  virtual void didAssociateFormControls(
      const blink::WebVector<blink::WebNode>& nodes) OVERRIDE;
  virtual void openTextDataListChooser(const blink::WebInputElement& element);

  void OnFormDataFilled(int query_id, const FormData& form);
  void OnFieldTypePredictionsAvailable(
      const std::vector<FormDataPredictions>& forms);

  // For external Autofill selection.
  void OnSetAutofillActionFill();
  void OnClearForm();
  void OnSetAutofillActionPreview();
  void OnClearPreviewedForm();
  void OnSetNodeText(const base::string16& value);
  void OnAcceptDataListSuggestion(const base::string16& value);
  void OnAcceptPasswordAutofillSuggestion(const base::string16& username);

  // Called when interactive autocomplete finishes.
  void OnRequestAutocompleteResult(
      blink::WebFormElement::AutocompleteResult result,
      const FormData& form_data);

  // Called when an autocomplete request succeeds or fails with the |result|.
  void FinishAutocompleteRequest(
      blink::WebFormElement::AutocompleteResult result);

  // Called when the page is actually shown in the browser, as opposed to simply
  // being preloaded.
  void OnPageShown();

  // Called in a posted task by textFieldDidChange() to work-around a WebKit bug
  // http://bugs.webkit.org/show_bug.cgi?id=16976
  void TextFieldDidChangeImpl(const blink::WebInputElement& element);

  // Shows the autofill suggestions for |element|.
  // This call is asynchronous and may or may not lead to the showing of a
  // suggestion popup (no popup is shown if there are no available suggestions).
  // |autofill_on_empty_values| specifies whether suggestions should be shown
  // when |element| contains no text.
  // |requires_caret_at_end| specifies whether suggestions should be shown when
  // the caret is not after the last character in |element|.
  // |display_warning_if_disabled| specifies whether a warning should be
  // displayed to the user if Autofill has suggestions available, but cannot
  // fill them because it is disabled (e.g. when trying to fill a credit card
  // form on a non-secure website).
  // |datalist_only| specifies whether all of <datalist> suggestions and no
  // autofill suggestions are shown. |autofill_on_empty_values| and
  // |requires_caret_at_end| are ignored if |datalist_only| is true.
  void ShowSuggestions(const blink::WebInputElement& element,
                       bool autofill_on_empty_values,
                       bool requires_caret_at_end,
                       bool display_warning_if_disabled,
                       bool datalist_only);

  // Queries the browser for Autocomplete and Autofill suggestions for the given
  // |element|.
  void QueryAutofillSuggestions(const blink::WebInputElement& element,
                                bool display_warning_if_disabled,
                                bool datalist_only);

  // Sets the element value to reflect the selected |suggested_value|.
  void AcceptDataListSuggestion(const base::string16& suggested_value);

  // Queries the AutofillManager for form data for the form containing |node|.
  // |value| is the current text in the field, and |unique_id| is the selected
  // profile's unique ID.  |action| specifies whether to Fill or Preview the
  // values returned from the AutofillManager.
  void FillAutofillFormData(const blink::WebNode& node,
                            int unique_id,
                            AutofillAction action);

  // Fills |form| and |field| with the FormData and FormField corresponding to
  // |node|. Returns true if the data was found; and false otherwise.
  bool FindFormAndFieldForNode(
      const blink::WebNode& node,
      FormData* form,
      FormFieldData* field) WARN_UNUSED_RESULT;

  // Set |node| to display the given |value|.
  void SetNodeText(const base::string16& value, blink::WebInputElement* node);

  // Hides any currently showing Autofill UI.
  void HideAutofillUI();

  FormCache form_cache_;

  PasswordAutofillAgent* password_autofill_agent_;  // Weak reference.
  PasswordGenerationAgent* password_generation_agent_;  // Weak reference.

  // The ID of the last request sent for form field Autofill.  Used to ignore
  // out of date responses.
  int autofill_query_id_;

  // The element corresponding to the last request sent for form field Autofill.
  blink::WebInputElement element_;

  // The form element currently requesting an interactive autocomplete.
  blink::WebFormElement in_flight_request_form_;

  // All the form elements seen in the top frame.
  std::vector<blink::WebFormElement> form_elements_;

  // The action to take when receiving Autofill data from the AutofillManager.
  AutofillAction autofill_action_;

  // Pointer to the WebView. Used to access page scale factor.
  blink::WebView* web_view_;

  // Should we display a warning if autofill is disabled?
  bool display_warning_if_disabled_;

  // Was the query node autofilled prior to previewing the form?
  bool was_query_node_autofilled_;

  // Have we already shown Autofill suggestions for the field the user is
  // currently editing?  Used to keep track of state for metrics logging.
  bool has_shown_autofill_popup_for_current_edit_;

  // If true we just set the node text so we shouldn't show the popup.
  bool did_set_node_text_;

  // Whether or not new forms/fields have been dynamically added
  // since the last loaded forms were sent to the browser process.
  bool has_new_forms_for_browser_;

  // Whether or not to ignore text changes.  Useful for when we're committing
  // a composition when we are defocusing the WebView and we don't want to
  // trigger an autofill popup to show.
  bool ignore_text_changes_;

  // Timestamp of first time forms are seen.
  base::TimeTicks forms_seen_timestamp_;

  base::WeakPtrFactory<AutofillAgent> weak_ptr_factory_;

  friend class PasswordAutofillAgentTest;
  FRIEND_TEST_ALL_PREFIXES(ChromeRenderViewTest, FillFormElement);
  FRIEND_TEST_ALL_PREFIXES(ChromeRenderViewTest, SendForms);
  FRIEND_TEST_ALL_PREFIXES(ChromeRenderViewTest, SendDynamicForms);
  FRIEND_TEST_ALL_PREFIXES(ChromeRenderViewTest, ShowAutofillWarning);
  FRIEND_TEST_ALL_PREFIXES(PasswordAutofillAgentTest, WaitUsername);
  FRIEND_TEST_ALL_PREFIXES(PasswordAutofillAgentTest, SuggestionAccept);
  FRIEND_TEST_ALL_PREFIXES(PasswordAutofillAgentTest, SuggestionSelect);
  FRIEND_TEST_ALL_PREFIXES(
      PasswordAutofillAgentTest,
      PasswordAutofillTriggersOnChangeEventsWaitForUsername);

  DISALLOW_COPY_AND_ASSIGN(AutofillAgent);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_AGENT_H_
