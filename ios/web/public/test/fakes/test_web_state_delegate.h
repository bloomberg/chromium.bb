// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_STATE_DELEGATE_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_STATE_DELEGATE_H_

#import <Foundation/Foundation.h>

#include <memory>
#include <set>

#import "ios/web/public/web_state/web_state_delegate.h"
#import "ios/web/public/test/fakes/test_java_script_dialog_presenter.h"

namespace web {

// Encapsulates parameters passed to CreateNewWebState.
struct TestCreateNewWebStateRequest {
  WebState* web_state = nullptr;
  GURL url;
  GURL opener_url;
  bool initiated_by_user = false;
};

// Encapsulates parameters passed to CloseWebState.
struct TestCloseWebStateRequest {
  WebState* web_state = nullptr;
};

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
  NSURLProtectionSpace* protection_space;
  NSURLCredential* credential;
  WebStateDelegate::AuthCallback auth_callback;
};

// Encapsulates information about popup.
struct TestPopup {
  TestPopup(const GURL& url, const GURL& opener_url)
      : url(url), opener_url(opener_url) {}
  GURL url;
  GURL opener_url;
};

// Fake WebStateDelegate used for testing purposes.
class TestWebStateDelegate : public WebStateDelegate {
 public:
  TestWebStateDelegate();
  ~TestWebStateDelegate() override;

  // WebStateDelegate overrides:
  WebState* CreateNewWebState(WebState* source,
                              const GURL& url,
                              const GURL& opener_url,
                              bool initiated_by_user) override;
  void CloseWebState(WebState* source) override;
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

  // Allows popups requested by a page with |opener_url|.
  void allow_popups(const GURL& opener_url) {
    allowed_popups_.insert(opener_url);
  }

  // Returns list of all child windows opened via CreateNewWebState.
  const std::vector<std::unique_ptr<WebState>>& child_windows() const {
    return child_windows_;
  }

  // Returns list of all popups requested via CreateNewWebState.
  const std::vector<TestPopup>& popups() const { return popups_; }

  // True if the WebStateDelegate HandleContextMenu method has been called.
  bool handle_context_menu_called() const {
    return handle_context_menu_called_;
  }

  // Returns the last Web State creation request passed to |CreateNewWebState|.
  TestCreateNewWebStateRequest* last_create_new_web_state_request() const {
    return last_create_new_web_state_request_.get();
  }

  // Returns the last Web State closing request passed to |CloseWebState|.
  TestCloseWebStateRequest* last_close_web_state_request() const {
    return last_close_web_state_request_.get();
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
  std::vector<std::unique_ptr<WebState>> child_windows_;
  // WebStates that were closed via |CloseWebState| callback.
  std::vector<std::unique_ptr<WebState>> closed_child_windows_;
  // A page can open popup if its URL is in this set.
  std::set<GURL> allowed_popups_;
  std::vector<TestPopup> popups_;
  bool handle_context_menu_called_ = false;
  std::unique_ptr<TestCreateNewWebStateRequest>
      last_create_new_web_state_request_;
  std::unique_ptr<TestCloseWebStateRequest> last_close_web_state_request_;
  std::unique_ptr<TestOpenURLRequest> last_open_url_request_;
  std::unique_ptr<TestRepostFormRequest> last_repost_form_request_;
  bool get_java_script_dialog_presenter_called_ = false;
  TestJavaScriptDialogPresenter java_script_dialog_presenter_;
  std::unique_ptr<TestAuthenticationRequest> last_authentication_request_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_STATE_DELEGATE_H_
