// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/athena_javascript_native_dialog_factory.h"

#include "base/memory/scoped_ptr.h"
#include "components/app_modal_dialogs/javascript_dialog_manager.h"
#include "components/app_modal_dialogs/javascript_native_dialog_factory.h"
#include "components/app_modal_dialogs/views/javascript_app_modal_dialog_views.h"
#include "components/constrained_window/constrained_window_views.h"

class JavaScriptAppModalDialog;
class NativeAppModalDialog;

namespace athena {
namespace {

class AthenaJavaScriptNativeDialogFactory
    : public JavaScriptNativeDialogFactory {
 public:
  AthenaJavaScriptNativeDialogFactory() {}
  ~AthenaJavaScriptNativeDialogFactory() override {}

 private:
  // JavScriptNativeDialogFactory:
  NativeAppModalDialog* CreateNativeJavaScriptDialog(
      JavaScriptAppModalDialog* dialog,
      gfx::NativeWindow parent_window) override{
    JavaScriptAppModalDialogViews* d =
        new JavaScriptAppModalDialogViews(dialog);
    constrained_window::CreateBrowserModalDialogViews(d, parent_window);
    return d;
  }

  DISALLOW_COPY_AND_ASSIGN(AthenaJavaScriptNativeDialogFactory);
};

}  // namespace

void InstallJavaScriptNativeDialogFactory() {
  SetJavaScriptNativeDialogFactory(
      make_scoped_ptr(new AthenaJavaScriptNativeDialogFactory));
}

}  // namespace athena
