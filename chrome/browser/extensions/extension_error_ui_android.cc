// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_error_ui_android.h"

#include "base/logging.h"

namespace extensions {

ExtensionErrorUIAndroid::ExtensionErrorUIAndroid(
    ExtensionErrorUI::Delegate* delegate) : ExtensionErrorUI(delegate) {}

ExtensionErrorUIAndroid::~ExtensionErrorUIAndroid() {}

// ExtensionErrorUI implementation:
bool ExtensionErrorUIAndroid::ShowErrorInBubbleView() {
  NOTIMPLEMENTED();
  return false;
}

void ExtensionErrorUIAndroid::ShowExtensions() {
  NOTIMPLEMENTED();
}

void ExtensionErrorUIAndroid::Close() {
  NOTIMPLEMENTED();
}

// static
ExtensionErrorUI* ExtensionErrorUI::Create(
    ExtensionErrorUI::Delegate* delegate) {
  return new ExtensionErrorUIAndroid(delegate);
}

}  // namespace extensions
