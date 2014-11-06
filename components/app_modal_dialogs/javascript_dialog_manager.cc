// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_modal_dialogs/javascript_dialog_manager.h"

#include "components/app_modal_dialogs/javascript_dialog_extensions_client.h"
#include "components/app_modal_dialogs/javascript_dialog_manager_impl.h"
#include "components/app_modal_dialogs/javascript_native_dialog_factory.h"

content::JavaScriptDialogManager* GetJavaScriptDialogManagerInstance() {
  return JavaScriptDialogManagerImpl::GetInstance();
}

void SetJavaScriptNativeDialogFactory(
    scoped_ptr<JavaScriptNativeDialogFactory> new_factory) {
  JavaScriptDialogManagerImpl::GetInstance()->SetNativeDialogFactory(
      new_factory.Pass());
}

void SetJavaScriptDialogExtensionsClient(
    scoped_ptr<JavaScriptDialogExtensionsClient> new_client) {
  JavaScriptDialogManagerImpl::GetInstance()->SetExtensionsClient(
      new_client.Pass());
}
