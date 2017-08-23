// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/test_web_state_delegate.h"

#include "base/memory/ptr_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

TestOpenURLRequest::TestOpenURLRequest()
    : params(GURL(),
             Referrer(),
             WindowOpenDisposition::UNKNOWN,
             ui::PAGE_TRANSITION_LINK,
             false) {}

TestOpenURLRequest::~TestOpenURLRequest() = default;

TestOpenURLRequest::TestOpenURLRequest(const TestOpenURLRequest&) = default;

TestRepostFormRequest::TestRepostFormRequest() {}

TestRepostFormRequest::~TestRepostFormRequest() = default;

TestRepostFormRequest::TestRepostFormRequest(const TestRepostFormRequest&) =
    default;

TestAuthenticationRequest::TestAuthenticationRequest() {}

TestAuthenticationRequest::~TestAuthenticationRequest() = default;

TestAuthenticationRequest::TestAuthenticationRequest(
    const TestAuthenticationRequest&) = default;

TestWebStateDelegate::TestWebStateDelegate() {}

TestWebStateDelegate::~TestWebStateDelegate() = default;

WebState* TestWebStateDelegate::CreateNewWebState(WebState* source,
                                                  const GURL& url,
                                                  const GURL& opener_url,
                                                  bool initiated_by_user) {
  last_create_new_web_state_request_ =
      base::MakeUnique<TestCreateNewWebStateRequest>();
  last_create_new_web_state_request_->web_state = source;
  last_create_new_web_state_request_->url = url;
  last_create_new_web_state_request_->opener_url = opener_url;
  last_create_new_web_state_request_->initiated_by_user = initiated_by_user;

  if (!initiated_by_user &&
      allowed_popups_.find(opener_url) == allowed_popups_.end()) {
    popups_.push_back(TestPopup(url, opener_url));
    return nullptr;
  }

  web::WebState::CreateParams params(source->GetBrowserState());
  params.created_with_opener = true;
  std::unique_ptr<web::WebState> child = web::WebState::Create(params);
  child->SetWebUsageEnabled(true);

  child_windows_.push_back(std::move(child));
  return child_windows_.back().get();
}

void TestWebStateDelegate::CloseWebState(WebState* source) {
  last_close_web_state_request_ = base::MakeUnique<TestCloseWebStateRequest>();
  last_close_web_state_request_->web_state = source;

  // Remove WebState from |child_windows_|.
  for (size_t i = 0; i < child_windows_.size(); i++) {
    if (child_windows_[i].get() == source) {
      closed_child_windows_.push_back(std::move(child_windows_[i]));
      child_windows_.erase(child_windows_.begin() + i);
      break;
    }
  }
}

WebState* TestWebStateDelegate::OpenURLFromWebState(
    WebState* web_state,
    const WebState::OpenURLParams& params) {
  last_open_url_request_ = base::MakeUnique<TestOpenURLRequest>();
  last_open_url_request_->web_state = web_state;
  last_open_url_request_->params = params;
  return nullptr;
}

JavaScriptDialogPresenter* TestWebStateDelegate::GetJavaScriptDialogPresenter(
    WebState*) {
  get_java_script_dialog_presenter_called_ = true;
  return &java_script_dialog_presenter_;
}

bool TestWebStateDelegate::HandleContextMenu(WebState*,
                                             const ContextMenuParams&) {
  handle_context_menu_called_ = true;
  return NO;
}

void TestWebStateDelegate::ShowRepostFormWarningDialog(
    WebState* source,
    const base::Callback<void(bool)>& callback) {
  last_repost_form_request_ = base::MakeUnique<TestRepostFormRequest>();
  last_repost_form_request_->web_state = source;
  last_repost_form_request_->callback = callback;
}

TestJavaScriptDialogPresenter*
TestWebStateDelegate::GetTestJavaScriptDialogPresenter() {
  return &java_script_dialog_presenter_;
}

void TestWebStateDelegate::OnAuthRequired(
    WebState* source,
    NSURLProtectionSpace* protection_space,
    NSURLCredential* credential,
    const AuthCallback& callback) {
  last_authentication_request_ = base::MakeUnique<TestAuthenticationRequest>();
  last_authentication_request_->web_state = source;
  last_authentication_request_->protection_space = protection_space;
  last_authentication_request_->credential = credential;
  last_authentication_request_->auth_callback = callback;
}

}  // namespace web
