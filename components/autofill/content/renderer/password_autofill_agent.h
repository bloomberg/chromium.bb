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

namespace WebKit {
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
  bool TextFieldDidEndEditing(const WebKit::WebInputElement& element);
  bool TextDidChangeInTextField(const WebKit::WebInputElement& element);
  bool TextFieldHandlingKeyDown(const WebKit::WebInputElement& element,
                                const WebKit::WebKeyboardEvent& event);

  // Fills the password associated with user name |value|. Returns true if the
  // username and password fields were filled, false otherwise.
  bool DidAcceptAutofillSuggestion(const WebKit::WebNode& node,
                                   const WebKit::WebString& value);
  // A no-op.  Password forms are not previewed, so they do not need to be
  // cleared when the selection changes.  However, this method returns
  // true when |node| is fillable by password Autofill.
  bool DidClearAutofillSelection(const WebKit::WebNode& node);
  // Shows an Autofill popup with username suggestions for |element|.
  // Returns true if any suggestions were shown, false otherwise.
  bool ShowSuggestions(const WebKit::WebInputElement& element);

  // Called when new form controls are inserted.
  void OnDynamicFormsSeen(WebKit::WebFrame* frame);

 protected:
  virtual bool OriginCanAccessPasswordManager(
      const WebKit::WebSecurityOrigin& origin);

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
    WebKit::WebInputElement password_field;
    PasswordFormFillData fill_data;
    bool backspace_pressed_last;
    PasswordInfo() : backspace_pressed_last(false) {}
  };
  typedef std::map<WebKit::WebElement, PasswordInfo> LoginToPasswordInfoMap;
  typedef std::map<WebKit::WebFrame*,
                   linked_ptr<PasswordForm> > FrameToPasswordFormMap;

  // RenderViewObserver:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidStartProvisionalLoad(WebKit::WebFrame* frame) OVERRIDE;
  virtual void DidStartLoading() OVERRIDE;
  virtual void DidFinishDocumentLoad(WebKit::WebFrame* frame) OVERRIDE;
  virtual void DidFinishLoad(WebKit::WebFrame* frame) OVERRIDE;
  virtual void FrameDetached(WebKit::WebFrame* frame) OVERRIDE;
  virtual void FrameWillClose(WebKit::WebFrame* frame) OVERRIDE;
  virtual void WillSendSubmitEvent(WebKit::WebFrame* frame,
                                   const WebKit::WebFormElement& form) OVERRIDE;
  virtual void WillSubmitForm(WebKit::WebFrame* frame,
                              const WebKit::WebFormElement& form) OVERRIDE;

  // RenderView IPC handlers:
  void OnFillPasswordForm(const PasswordFormFillData& form_data);

  // Scans the given frame for password forms and sends them up to the browser.
  // If |only_visible| is true, only forms visible in the layout are sent.
  void SendPasswordForms(WebKit::WebFrame* frame, bool only_visible);

  void GetSuggestions(const PasswordFormFillData& fill_data,
                      const base::string16& input,
                      std::vector<base::string16>* suggestions,
                      std::vector<base::string16>* realms);

  bool ShowSuggestionPopup(const PasswordFormFillData& fill_data,
                           const WebKit::WebInputElement& user_input);

  // Attempts to fill |username_element| and |password_element| with the
  // |fill_data|.  Will use the data corresponding to the preferred username,
  // unless the |username_element| already has a value set.  In that case,
  // attempts to fill the password matching the already filled username, if
  // such a password exists.
  void FillFormOnPasswordRecieved(const PasswordFormFillData& fill_data,
                                  WebKit::WebInputElement username_element,
                                  WebKit::WebInputElement password_element);

  bool FillUserNameAndPassword(
      WebKit::WebInputElement* username_element,
      WebKit::WebInputElement* password_element,
      const PasswordFormFillData& fill_data,
      bool exact_username_match,
      bool set_selection);

  // Fills |login_input| and |password| with the most relevant suggestion from
  // |fill_data| and shows a popup with other suggestions.
  void PerformInlineAutocomplete(
      const WebKit::WebInputElement& username,
      const WebKit::WebInputElement& password,
      const PasswordFormFillData& fill_data);

  // Invoked when the passed frame is closing.  Gives us a chance to clear any
  // reference we may have to elements in that frame.
  void FrameClosing(const WebKit::WebFrame* frame);

  // Finds login information for a |node| that was previously filled.
  bool FindLoginInfo(const WebKit::WebNode& node,
                     WebKit::WebInputElement* found_input,
                     PasswordInfo* found_password);

  // The logins we have filled so far with their associated info.
  LoginToPasswordInfoMap login_to_password_info_;

  // Used for UMA stats.
  OtherPossibleUsernamesUsage usernames_usage_;

  // Pointer to the WebView. Used to access page scale factor.
  WebKit::WebView* web_view_;

  // Set if the user might be submitting a password form on the current page,
  // but the submit may still fail (i.e. doesn't pass JavaScript validation).
  FrameToPasswordFormMap provisionally_saved_forms_;

  base::WeakPtrFactory<PasswordAutofillAgent> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PasswordAutofillAgent);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_AUTOFILL_AGENT_H_
