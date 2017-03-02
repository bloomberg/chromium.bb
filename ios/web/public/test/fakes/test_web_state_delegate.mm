// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/test_web_state_delegate.h"

#include "base/memory/ptr_util.h"

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
  last_authentication_request_->protection_space.reset(
      [protection_space retain]);
  last_authentication_request_->credential.reset([credential retain]);
  last_authentication_request_->auth_callback = callback;
}

}  // namespace web
