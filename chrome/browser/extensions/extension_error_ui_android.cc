// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_error_ui_android.h"

ExtensionErrorUIAndroid::ExtensionErrorUIAndroid(
    ExtensionService* extension_service)
    : ExtensionErrorUI(extension_service) {
}

ExtensionErrorUIAndroid::~ExtensionErrorUIAndroid() {
}

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
    ExtensionService* extension_service) {
  return new ExtensionErrorUIAndroid(extension_service);
}
