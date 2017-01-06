// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_STATE_DELEGATE_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_STATE_DELEGATE_H_

#include "ios/web/public/web_state/web_state_delegate.h"
#import "ios/web/public/test/fakes/test_java_script_dialog_presenter.h"

namespace web {

// Fake WebStateDelegate used for testing purposes.
class TestWebStateDelegate : public WebStateDelegate {
 public:
  // WebStateDelegate overrides:
  JavaScriptDialogPresenter* GetJavaScriptDialogPresenter(WebState*) override;
  void LoadProgressChanged(WebState* source, double progress) override;
  bool HandleContextMenu(WebState* source,
                         const ContextMenuParams& params) override;

  TestJavaScriptDialogPresenter* GetTestJavaScriptDialogPresenter();

  // True if the WebStateDelegate LoadProgressChanged method has been called.
  bool load_progress_changed_called() const {
    return load_progress_changed_called_;
  }

  // True if the WebStateDelegate HandleContextMenu method has been called.
  bool handle_context_menu_called() const {
    return handle_context_menu_called_;
  }

  // True if the WebStateDelegate GetJavaScriptDialogPresenter method has been
  // called.
  bool get_java_script_dialog_presenter_called() const {
    return get_java_script_dialog_presenter_called_;
  }

 private:
  bool load_progress_changed_called_ = false;
  bool handle_context_menu_called_ = false;
  bool get_java_script_dialog_presenter_called_ = false;
  TestJavaScriptDialogPresenter java_script_dialog_presenter_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_STATE_DELEGATE_H_
