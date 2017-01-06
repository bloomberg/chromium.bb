// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/test_web_state_delegate.h"

namespace web {

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

}  // namespace web
