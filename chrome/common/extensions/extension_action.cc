// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/extensions/extension_action.h"

int ExtensionAction::next_command_id_ = IDC_BROWSER_ACTION_FIRST;

ExtensionAction::ExtensionAction()
  : type_(PAGE_ACTION), command_id_(next_command_id_++) {
}

ExtensionAction::~ExtensionAction() {
}
