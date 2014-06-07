// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/guest_view/guest_view_internal_api.h"

#include "chrome/browser/guest_view/guest_view_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/guest_view_internal.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

GuestViewInternalAllocateInstanceIdFunction::
    GuestViewInternalAllocateInstanceIdFunction() {
}

bool GuestViewInternalAllocateInstanceIdFunction::RunAsync() {
  EXTENSION_FUNCTION_VALIDATE(!args_->GetSize());

  // Check if we have "webview" permission.
  CHECK(GetExtension()->permissions_data()->HasAPIPermission(
            APIPermission::kWebView));

  int instanceId = GuestViewManager::FromBrowserContext(browser_context())
                       ->GetNextInstanceID();
  SetResult(base::Value::CreateIntegerValue(instanceId));
  SendResponse(true);
  return true;
}

}  // namespace extensions
