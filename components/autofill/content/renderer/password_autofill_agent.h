// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_AUTOFILL_AGENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_AUTOFILL_AGENT_H_

#include <map>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/public/web/WebInputElement.h"

namespace blink {
class WebInputElement;
class WebKeyboardEvent;
class WebSecurityOrigin;
class WebView;
}

namespace autofill {

// This class is responsible for filling password forms.
// There is one PasswordAutofillAgent per RenderView.
class PasswordAutofillAgent : public content::RenderViewObserver {
 public:
  explicit PasswordAutofillAgent(content::RenderView* render_view);
  virtual ~PasswordAutofillAgent();

  // WebViewClient editor related calls forwarded by the RenderView.
  // If they return true, it indicates the event was consumed and should not
  // be used for any other autofill activity.
  bool TextFieldDidEndEditing(const blink::WebInputElement& element);
  bool TextDidChangeInTextField(const blink::WebInputElement& element);
  bool TextFieldHandlingKeyDown(const blink::WebInputElement& element,
                                const blink::WebKeyboardEvent& event);

  // Fills the password associated with user name |username|. Returns true if
  // the username and password fields were filled, false otherwise.
  bool DidAcceptAutofillSuggestion(const blink::WebNode& node,
                                   const blink::WebString& username);
  // A no-op.  Password forms are not previewed, so they do not need to be
  // cleared when the selection changes.  However, this method returns
  // true when |node| is fillable by password Autofill.
  bool DidClearAutofillSelection(const blink::WebNode& node);
  // Shows an Autofill popup with username suggestions for |element|.
  // Returns true if any suggestions were shown, false otherwise.
  bool ShowSuggestions(const blink::WebInputElement& element);

  // Called when new form controls are inserted.
  void OnDynamicFormsSeen(blink::WebFrame* frame);

 protected:
  virtual bool OriginCanAccessPasswordManager(
      const blink::WebSecurityOrigin& origin);

 private:
  friend class PasswordAutofillAgentTest;

  enum OtherPossibleUsernamesUsage {
    NOTHING_TO_AUTOFILL,
    OTHER_POSSIBLE_USERNAMES_ABSENT,
    OTHER_POSSIBLE_USERNAMES_PRESENT,
    OTHER_POSSIBLE_USERNAME_SHOWN,
    OTHER_POSSIBLE_USERNAME_SELECTED,
    OTHER_POSSIBLE_USERNAMES_MAX
  };

  struct PasswordInfo {
    blink::WebInputElement password_field;
    PasswordFormFillData fill_data;
    bool backspace_pressed_last;
    PasswordInfo() : backspace_pressed_last(false) {}
  };
  typedef std::map<blink::WebElement, PasswordInfo> LoginToPasswordInfoMap;
  typedef std::map<blink::WebFrame*,
                   linked_ptr<PasswordForm> > FrameToPasswordFormMap;

  // This class holds a vector of autofilled password input elements and makes
  // sure the autofilled password value is not accessible to JavaScript code
  // until the user interacts with the page.
  class PasswordValueGatekeeper {
   public:
    PasswordValueGatekeeper();
    ~PasswordValueGatekeeper();

    // Call this for every autofilled password field, so that the gatekeeper
    // protects the value accordingly.
    void RegisterElement(blink::WebInputElement* element);

    // Call this to notify the gatekeeper that the user interacted with the
    // page.
    void OnUserGesture();

    // Call this to reset the gatekeeper on a new page navigation.
    void Reset();

   private:
    // Make the value of |element| accessible to JavaScript code.
    void ShowValue(blink::WebInputElement* element);

    bool was_user_gesture_seen_;
    std::vector<blink::WebInputElement> elements_;

    DISALLOW_COPY_AND_ASSIGN(PasswordValueGatekeeper);
  };

  // RenderViewObserver:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidStartProvisionalLoad(blink::WebLocalFrame* frame) OVERRIDE;
  virtual void DidStartLoading() OVERRIDE;
  virtual void DidFinishDocumentLoad(blink::WebLocalFrame* frame) OVERRIDE;
  virtual void DidFinishLoad(blink::WebLocalFrame* frame) OVERRIDE;
  virtual void FrameDetached(blink::WebFrame* frame) OVERRIDE;
  virtual void FrameWillClose(blink::WebFrame* frame) OVERRIDE;
  virtual void WillSendSubmitEvent(blink::WebLocalFrame* frame,
                                   const blink::WebFormElement& form) OVERRIDE;
  virtual void WillSubmitForm(blink::WebLocalFrame* frame,
                              const blink::WebFormElement& form) OVERRIDE;
  virtual void WillProcessUserGesture() OVERRIDE;

  // RenderView IPC handlers:
  void OnFillPasswordForm(const PasswordFormFillData& form_data);

  // Scans the given frame for password forms and sends them up to the browser.
  // If |only_visible| is true, only forms visible in the layout are sent.
  void SendPasswordForms(blink::WebFrame* frame, bool only_visible);

  void GetSuggestions(const PasswordFormFillData& fill_data,
                      const base::string16& input,
                      std::vector<base::string16>* suggestions,
                      std::vector<base::string16>* realms);

  bool ShowSuggestionPopup(const PasswordFormFillData& fill_data,
                           const blink::WebInputElement& user_input);

  // Attempts to fill |username_element| and |password_element| with the
  // |fill_data|.  Will use the data corresponding to the preferred username,
  // unless the |username_element| already has a value set.  In that case,
  // attempts to fill the password matching the already filled username, if
  // such a password exists.
  void FillFormOnPasswordRecieved(const PasswordFormFillData& fill_data,
                                  blink::WebInputElement username_element,
                                  blink::WebInputElement password_element);

  bool FillUserNameAndPassword(
      blink::WebInputElement* username_element,
      blink::WebInputElement* password_element,
      const PasswordFormFillData& fill_data,
      bool exact_username_match,
      bool set_selection);

  // Fills |login_input| and |password| with the most relevant suggestion from
  // |fill_data| and shows a popup with other suggestions.
  void PerformInlineAutocomplete(
      const blink::WebInputElement& username,
      const blink::WebInputElement& password,
      const PasswordFormFillData& fill_data);

  // Invoked when the passed frame is closing.  Gives us a chance to clear any
  // reference we may have to elements in that frame.
  void FrameClosing(const blink::WebFrame* frame);

  // Finds login information for a |node| that was previously filled.
  bool FindLoginInfo(const blink::WebNode& node,
                     blink::WebInputElement* found_input,
                     PasswordInfo* found_password);

  // If |provisionally_saved_forms_| contains a form for |current_frame| or its
  // children, return such frame.
  blink::WebFrame* CurrentOrChildFrameWithSavedForms(
      const blink::WebFrame* current_frame);

  // The logins we have filled so far with their associated info.
  LoginToPasswordInfoMap login_to_password_info_;

  // Used for UMA stats.
  OtherPossibleUsernamesUsage usernames_usage_;

  // Pointer to the WebView. Used to access page scale factor.
  blink::WebView* web_view_;

  // Set if the user might be submitting a password form on the current page,
  // but the submit may still fail (i.e. doesn't pass JavaScript validation).
  FrameToPasswordFormMap provisionally_saved_forms_;

  PasswordValueGatekeeper gatekeeper_;

  base::WeakPtrFactory<PasswordAutofillAgent> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PasswordAutofillAgent);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_AUTOFILL_AGENT_H_
