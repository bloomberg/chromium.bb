// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_modal_dialogs/chrome_javascript_native_dialog_factory.h"

#include "components/app_modal_dialogs/javascript_dialog_manager.h"
#include "components/app_modal_dialogs/javascript_native_dialog_factory.h"
#include "components/constrained_window/constrained_window_views.h"

#if defined(USE_X11) && !defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/javascript_app_modal_dialog_views_x11.h"
#else
#include "components/app_modal_dialogs/views/javascript_app_modal_dialog_views.h"
#endif

namespace {

class ChromeJavaScriptNativeDialogViewsFactory
    : public JavaScriptNativeDialogFactory {
 public:
  ChromeJavaScriptNativeDialogViewsFactory() {}
  ~ChromeJavaScriptNativeDialogViewsFactory() override {}

 private:
  NativeAppModalDialog* CreateNativeJavaScriptDialog(
      JavaScriptAppModalDialog* dialog,
      gfx::NativeWindow parent_window) override{
    JavaScriptAppModalDialogViews* d = nullptr;
#if defined(USE_X11) && !defined(OS_CHROMEOS)
    d = new JavaScriptAppModalDialogViewsX11(dialog);
#else
    d = new JavaScriptAppModalDialogViews(dialog);
#endif
    CreateBrowserModalDialogViews(d, parent_window);
    return d;
  }

  DISALLOW_COPY_AND_ASSIGN(ChromeJavaScriptNativeDialogViewsFactory);
};

}  // namespace

void InstallChromeJavaScriptNativeDialogFactory() {
  SetJavaScriptNativeDialogFactory(
      make_scoped_ptr(new ChromeJavaScriptNativeDialogViewsFactory));
}
