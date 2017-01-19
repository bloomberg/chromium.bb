// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/test_web_state_delegate.h"

#include "base/memory/ptr_util.h"

namespace web {

TestAuthenticationRequest::TestAuthenticationRequest() {}

TestAuthenticationRequest::~TestAuthenticationRequest() = default;

TestWebStateDelegate::TestWebStateDelegate() {}

TestWebStateDelegate::~TestWebStateDelegate() = default;

JavaScriptDialogPresenter* TestWebStateDelegate::GetJavaScriptDialogPresenter(
    WebState*) {
  get_java_script_dialog_presenter_called_ = true;
  return &java_script_dialog_presenter_;
}

void TestWebStateDelegate::LoadProgressChanged(WebState*, double) {
  load_progress_changed_called_ = true;
}

bool TestWebStateDelegate::HandleContextMenu(WebState*,
                                             const ContextMenuParams&) {
  handle_context_menu_called_ = true;
  return NO;
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
