// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_STATE_DELEGATE_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_STATE_DELEGATE_H_

#import <Foundation/Foundation.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"
#import "ios/web/public/web_state/web_state_delegate.h"
#import "ios/web/public/test/fakes/test_java_script_dialog_presenter.h"

namespace web {

// Encapsulates parameters passed to OpenURLFromWebState.
struct TestOpenURLRequest {
  TestOpenURLRequest();
  TestOpenURLRequest(const TestOpenURLRequest&);
  ~TestOpenURLRequest();
  WebState* web_state = nullptr;
  WebState::OpenURLParams params;
};

// Encapsulates parameters passed to ShowRepostFormWarningDialog.
struct TestRepostFormRequest {
  TestRepostFormRequest();
  TestRepostFormRequest(const TestRepostFormRequest&);
  ~TestRepostFormRequest();
  WebState* web_state = nullptr;
  base::Callback<void(bool)> callback;
};

// Encapsulates parameters passed to OnAuthRequired.
struct TestAuthenticationRequest {
  TestAuthenticationRequest();
  TestAuthenticationRequest(const TestAuthenticationRequest&);
  ~TestAuthenticationRequest();
  WebState* web_state = nullptr;
  base::scoped_nsobject<NSURLProtectionSpace> protection_space;
  base::scoped_nsobject<NSURLCredential> credential;
  WebStateDelegate::AuthCallback auth_callback;
};

// Fake WebStateDelegate used for testing purposes.
class TestWebStateDelegate : public WebStateDelegate {
 public:
  TestWebStateDelegate();
  ~TestWebStateDelegate() override;

  // WebStateDelegate overrides:
  WebState* OpenURLFromWebState(WebState*,
                                const WebState::OpenURLParams&) override;
  JavaScriptDialogPresenter* GetJavaScriptDialogPresenter(WebState*) override;
  bool HandleContextMenu(WebState* source,
                         const ContextMenuParams& params) override;
  void ShowRepostFormWarningDialog(
      WebState* source,
      const base::Callback<void(bool)>& callback) override;
  TestJavaScriptDialogPresenter* GetTestJavaScriptDialogPresenter();
  void OnAuthRequired(WebState* source,
                      NSURLProtectionSpace* protection_space,
                      NSURLCredential* proposed_credential,
                      const AuthCallback& callback) override;

  // True if the WebStateDelegate HandleContextMenu method has been called.
  bool handle_context_menu_called() const {
    return handle_context_menu_called_;
  }

  // Returns the last Open URL request passed to |OpenURLFromWebState|.
  TestOpenURLRequest* last_open_url_request() const {
    return last_open_url_request_.get();
  }

  // Returns the last Repost Form request passed to
  // |ShowRepostFormWarningDialog|.
  TestRepostFormRequest* last_repost_form_request() const {
    return last_repost_form_request_.get();
  }

  // True if the WebStateDelegate GetJavaScriptDialogPresenter method has been
  // called.
  bool get_java_script_dialog_presenter_called() const {
    return get_java_script_dialog_presenter_called_;
  }

  // Returns the last HTTP Authentication request passed to |OnAuthRequired|.
  TestAuthenticationRequest* last_authentication_request() const {
    return last_authentication_request_.get();
  }

  // Clears the last HTTP Authentication request passed to |OnAuthRequired|.
  void ClearLastAuthenticationRequest() {
    last_authentication_request_.reset();
  }

 private:
  bool handle_context_menu_called_ = false;
  std::unique_ptr<TestOpenURLRequest> last_open_url_request_;
  std::unique_ptr<TestRepostFormRequest> last_repost_form_request_;
  bool get_java_script_dialog_presenter_called_ = false;
  TestJavaScriptDialogPresenter java_script_dialog_presenter_;
  std::unique_ptr<TestAuthenticationRequest> last_authentication_request_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_STATE_DELEGATE_H_
