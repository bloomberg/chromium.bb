// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/athena_javascript_native_dialog_factory.h"

#include "base/memory/scoped_ptr.h"
#include "components/app_modal/javascript_dialog_manager.h"
#include "components/app_modal/javascript_native_dialog_factory.h"
#include "components/app_modal/views/javascript_app_modal_dialog_views.h"
#include "components/constrained_window/constrained_window_views.h"

class JavaScriptAppModalDialog;
class NativeAppModalDialog;

namespace athena {
namespace {

class AthenaJavaScriptNativeDialogFactory
    : public app_modal::JavaScriptNativeDialogFactory {
 public:
  AthenaJavaScriptNativeDialogFactory() {}
  ~AthenaJavaScriptNativeDialogFactory() override {}

 private:
  // app_modal::JavScriptNativeDialogFactory:
  app_modal::NativeAppModalDialog* CreateNativeJavaScriptDialog(
      app_modal::JavaScriptAppModalDialog* dialog,
      gfx::NativeWindow parent_window) override{
    app_modal::JavaScriptAppModalDialogViews* d =
        new app_modal::JavaScriptAppModalDialogViews(dialog);
    constrained_window::CreateBrowserModalDialogViews(d, parent_window);
    return d;
  }

  DISALLOW_COPY_AND_ASSIGN(AthenaJavaScriptNativeDialogFactory);
};

}  // namespace

void InstallJavaScriptNativeDialogFactory() {
  app_modal::JavaScriptDialogManager::GetInstance()->
      SetNativeDialogFactory(
          make_scoped_ptr(new AthenaJavaScriptNativeDialogFactory));
}

}  // namespace athena
