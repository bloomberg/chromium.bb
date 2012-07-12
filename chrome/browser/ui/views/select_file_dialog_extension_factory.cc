// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/select_file_dialog_extension_factory.h"

#include "chrome/browser/ui/views/select_file_dialog_extension.h"

SelectFileDialogExtensionFactory::SelectFileDialogExtensionFactory() {
}

SelectFileDialogExtensionFactory::~SelectFileDialogExtensionFactory() {
}

ui::SelectFileDialog* SelectFileDialogExtensionFactory::Create(
    ui::SelectFileDialog::Listener* listener,
    ui::SelectFilePolicy* policy) {
  return SelectFileDialogExtension::Create(listener, policy);
}
