// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PASSWORD_AUTOCOMPLETE_MANAGER_H_
#define CHROME_RENDERER_PASSWORD_AUTOCOMPLETE_MANAGER_H_

#include <map>
#include <vector>

#include "base/task.h"
#include "webkit/glue/password_form_dom_manager.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputElement.h"

class RenderView;
namespace WebKit {
class WebKeyboardEvent;
class WebView;
}

// This class is responsible for filling password forms.
// There is one PasswordAutocompleteManager per RenderView.
class PasswordAutocompleteManager {
 public:
  explicit PasswordAutocompleteManager(RenderView* render_view);
  virtual ~PasswordAutocompleteManager() {}

  // Invoked by the renderer when it receives the password info from the
  // browser.  This triggers a password autocomplete (if wait_for_username is
  // false on |form_data|).
  void ReceivedPasswordFormFillData(WebKit::WebView* view,
      const webkit_glue::PasswordFormDomManager::FillData& form_data);

  // Invoked when the passed frame is closing.  Gives us a chance to clear any
  // reference we may have to elements in that frame.
  void FrameClosing(const WebKit::WebFrame* frame);

  // Fills the password associated with |user_input|, using its current value
  // as the actual user name.  Returns true if the password field was filled,
  // false otherwise, typically if there was no matching suggestions for the
  // currently typed username.
  bool FillPassword(const WebKit::WebInputElement& user_input);

  // Fills |login_input| and |password| with the most relevant suggestion from
  // |fill_data| and shows a popup with other suggestions.
  void PerformInlineAutocomplete(
      const WebKit::WebInputElement& username,
      const WebKit::WebInputElement& password,
      const webkit_glue::PasswordFormDomManager::FillData& fill_data);

  // WebViewClient editor related calls forwarded by the RenderView.
  void TextFieldDidBeginEditing(const WebKit::WebInputElement& element);
  void TextFieldDidEndEditing(const WebKit::WebInputElement& element);
  void TextDidChangeInTextField(const WebKit::WebInputElement& element);
  void TextFieldHandlingKeyDown(const WebKit::WebInputElement& element,
                                const WebKit::WebKeyboardEvent& event);

 private:
  struct PasswordInfo {
    WebKit::WebInputElement password_field;
    webkit_glue::PasswordFormDomManager::FillData fill_data;
    bool backspace_pressed_last;
    PasswordInfo() : backspace_pressed_last(false) {}
  };
  typedef std::map<WebKit::WebElement, PasswordInfo> LoginToPasswordInfoMap;

  void GetSuggestions(
      const webkit_glue::PasswordFormDomManager::FillData& fill_data,
      const string16& input,
      std::vector<string16>* suggestions);

  bool ShowSuggestionPopup(
      const webkit_glue::PasswordFormDomManager::FillData& fill_data,
      const WebKit::WebInputElement& user_input);

  bool FillUserNameAndPassword(
      WebKit::WebInputElement* username_element,
      WebKit::WebInputElement* password_element,
      const webkit_glue::PasswordFormDomManager::FillData& fill_data,
      bool exact_username_match);

  // The logins we have filled so far with their associated info.
  LoginToPasswordInfoMap login_to_password_info_;

  ScopedRunnableMethodFactory<PasswordAutocompleteManager> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(PasswordAutocompleteManager);
};

#endif  // CHROME_RENDERER_PASSWORD_AUTOCOMPLETE_MANAGER_H_
