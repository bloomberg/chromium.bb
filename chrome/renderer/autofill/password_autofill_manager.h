// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_AUTOFILL_PASSWORD_AUTOFILL_MANAGER_H_
#define CHROME_RENDERER_AUTOFILL_PASSWORD_AUTOFILL_MANAGER_H_
#pragma once

#include <map>
#include <vector>

#include "base/task.h"
#include "chrome/renderer/page_click_listener.h"
#include "chrome/renderer/render_view_observer.h"
#include "webkit/glue/password_form_dom_manager.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"

namespace WebKit {
class WebInputElement;
class WebKeyboardEvent;
}

namespace autofill {

// This class is responsible for filling password forms.
// There is one PasswordAutoFillManager per RenderView.
class PasswordAutoFillManager : public RenderViewObserver,
                                public PageClickListener {
 public:
  explicit PasswordAutoFillManager(RenderView* render_view);
  virtual ~PasswordAutoFillManager();

  // WebViewClient editor related calls forwarded by the RenderView.
  // If they return true, it indicates the event was consumed and should not
  // be used for any other autofill activity.
  bool TextFieldDidEndEditing(const WebKit::WebInputElement& element);
  bool TextDidChangeInTextField(const WebKit::WebInputElement& element);
  bool TextFieldHandlingKeyDown(const WebKit::WebInputElement& element,
                                const WebKit::WebKeyboardEvent& event);
  // Fills the password associated with user name |value|. Returns true if the
  // username and password fields were filled, false otherwise.
  bool DidSelectAutoFillSuggestion(const WebKit::WebNode& node,
                                   const WebKit::WebString& value);

 private:
  friend class PasswordAutoFillManagerTest;

  struct PasswordInfo {
    WebKit::WebInputElement password_field;
    webkit_glue::PasswordFormFillData fill_data;
    bool backspace_pressed_last;
    PasswordInfo() : backspace_pressed_last(false) {}
  };
  typedef std::map<WebKit::WebElement, PasswordInfo> LoginToPasswordInfoMap;

  // RenderViewObserver:
  virtual bool OnMessageReceived(const IPC::Message& message);
  virtual void DidFinishDocumentLoad(WebKit::WebFrame* frame);
  virtual void DidFinishLoad(WebKit::WebFrame* frame);
  virtual void FrameDetached(WebKit::WebFrame* frame);
  virtual void FrameWillClose(WebKit::WebFrame* frame);

  // PageClickListener:
  virtual bool InputElementClicked(const WebKit::WebInputElement& element,
                                   bool was_focused,
                                   bool is_focused);

  // RenderView IPC handlers:
  void OnFillPasswordForm(const webkit_glue::PasswordFormFillData& form_data);

  // Scans the given frame for password forms and sends them up to the browser.
  // If |only_visible| is true, only forms visible in the layout are sent.
  void SendPasswordForms(WebKit::WebFrame* frame, bool only_visible);

  void GetSuggestions(const webkit_glue::PasswordFormFillData& fill_data,
                      const string16& input,
                      std::vector<string16>* suggestions);

  bool ShowSuggestionPopup(const webkit_glue::PasswordFormFillData& fill_data,
                           const WebKit::WebInputElement& user_input);

  bool FillUserNameAndPassword(
      WebKit::WebInputElement* username_element,
      WebKit::WebInputElement* password_element,
      const webkit_glue::PasswordFormFillData& fill_data,
      bool exact_username_match,
      bool set_selection);

  // Fills |login_input| and |password| with the most relevant suggestion from
  // |fill_data| and shows a popup with other suggestions.
  void PerformInlineAutocomplete(
      const WebKit::WebInputElement& username,
      const WebKit::WebInputElement& password,
      const webkit_glue::PasswordFormFillData& fill_data);

  // Invoked when the passed frame is closing.  Gives us a chance to clear any
  // reference we may have to elements in that frame.
  void FrameClosing(const WebKit::WebFrame* frame);

  // The logins we have filled so far with their associated info.
  LoginToPasswordInfoMap login_to_password_info_;

  ScopedRunnableMethodFactory<PasswordAutoFillManager> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(PasswordAutoFillManager);
};

}  // namespace autofill

#endif  // CHROME_RENDERER_AUTOFILL_PASSWORD_AUTOFILL_MANAGER_H_
