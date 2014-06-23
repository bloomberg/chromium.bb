// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/guest_view/guest_view_internal_api.h"

#include "chrome/browser/guest_view/guest_view_base.h"
#include "chrome/browser/guest_view/guest_view_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/guest_view_internal.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/common/permissions/permissions_data.h"

namespace {
const char* kWebViewPermissionRequiredError =
    "\"webview\" permission is required for allocating instance ID.";
}  // namespace

namespace extensions {

GuestViewInternalCreateGuestFunction::
    GuestViewInternalCreateGuestFunction() {
}

bool GuestViewInternalCreateGuestFunction::RunAsync() {
  std::string view_type;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &view_type));

  base::DictionaryValue* create_params;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &create_params));

  if (!GetExtension()->permissions_data()->HasAPIPermission(
          APIPermission::kWebView)) {
    LOG(ERROR) << kWebViewPermissionRequiredError;
    error_ = kWebViewPermissionRequiredError;
    SendResponse(false);
  }

  GuestViewManager* guest_view_manager =
      GuestViewManager::FromBrowserContext(browser_context());

  content::WebContents* guest_web_contents =
      guest_view_manager->CreateGuest(view_type,
                                      extension_id(),
                                      render_view_host()->GetProcess()->GetID(),
                                      *create_params);
  if (!guest_web_contents)
    return false;

  GuestViewBase* guest = GuestViewBase::FromWebContents(guest_web_contents);
  SetResult(base::Value::CreateIntegerValue(guest->GetGuestInstanceID()));
  SendResponse(true);
  return true;
}

}  // namespace extensions
