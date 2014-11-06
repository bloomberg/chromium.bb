// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/public/extensions_delegate.h"

#include "athena/extensions/athena_constrained_window_views_client.h"
#include "athena/extensions/athena_javascript_native_dialog_factory.h"
#include "base/logging.h"
#include "extensions/components/javascript_dialog_extensions_client/javascript_dialog_extension_client_impl.h"

namespace athena {
namespace {

ExtensionsDelegate* instance = nullptr;

}  // namespace

ExtensionsDelegate::ExtensionsDelegate() {
  InstallConstrainedWindowViewsClient();
  InstallJavaScriptDialogExtensionsClient();
  InstallJavaScriptNativeDialogFactory();
  DCHECK(!instance);
  instance = this;
}

ExtensionsDelegate::~ExtensionsDelegate() {
  UninstallConstrainedWindowViewsClient();
  DCHECK(instance);
  instance = nullptr;
}

// static
ExtensionsDelegate* ExtensionsDelegate::Get(content::BrowserContext* context) {
  DCHECK(instance);
  DCHECK_EQ(context, instance->GetBrowserContext());
  return instance;
}

// static
void ExtensionsDelegate::Shutdown() {
  DCHECK(instance);
  delete instance;
}

}  // namespace athena
