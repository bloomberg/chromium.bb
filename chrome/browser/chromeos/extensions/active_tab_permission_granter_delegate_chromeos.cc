// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/active_tab_permission_granter_delegate_chromeos.h"

#include "chrome/browser/chromeos/extensions/public_session_permission_helper.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"

namespace extensions {

ActiveTabPermissionGranterDelegateChromeOS::
    ActiveTabPermissionGranterDelegateChromeOS() {}

ActiveTabPermissionGranterDelegateChromeOS::
    ~ActiveTabPermissionGranterDelegateChromeOS() {}

bool ActiveTabPermissionGranterDelegateChromeOS::ShouldGrantActiveTab(
    const Extension* extension,
    content::WebContents* web_contents) {
  bool already_handled = permission_helper::HandlePermissionRequest(
      *extension, {APIPermission::kActiveTab}, web_contents,
      permission_helper::RequestResolvedCallback(),
      permission_helper::PromptFactory());

  return already_handled && permission_helper::PermissionAllowed(
                                extension, APIPermission::kActiveTab);
}

}  // namespace extensions
