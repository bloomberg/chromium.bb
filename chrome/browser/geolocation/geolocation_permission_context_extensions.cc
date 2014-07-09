// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context_extensions.h"

#include "base/callback.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/content_settings/permission_request_id.h"
#include "chrome/browser/extensions/suggest_permission_util.h"
#include "chrome/browser/guest_view/web_view/web_view_permission_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"

using extensions::APIPermission;
using extensions::ExtensionRegistry;
#endif

GeolocationPermissionContextExtensions::
GeolocationPermissionContextExtensions(Profile* profile)
    : profile_(profile) {
}

GeolocationPermissionContextExtensions::
~GeolocationPermissionContextExtensions() {
}

bool GeolocationPermissionContextExtensions::RequestPermission(
    content::WebContents* web_contents,
    const PermissionRequestID& request_id,
    int bridge_id,
    const GURL& requesting_frame,
    bool user_gesture,
    base::Callback<void(bool)> callback,
    bool* permission_set,
    bool* new_permission) {
#if defined(ENABLE_EXTENSIONS)
  GURL requesting_frame_origin = requesting_frame.GetOrigin();

  WebViewPermissionHelper* web_view_permission_helper =
      WebViewPermissionHelper::FromWebContents(web_contents);
  if (web_view_permission_helper) {
    web_view_permission_helper->RequestGeolocationPermission(
        bridge_id, requesting_frame, user_gesture, callback);
    *permission_set = false;
    *new_permission = false;
    return true;
  }

  ExtensionRegistry* extension_registry = ExtensionRegistry::Get(profile_);
  if (extension_registry) {
    const extensions::Extension* extension =
        extension_registry->enabled_extensions().GetExtensionOrAppByURL(
            requesting_frame_origin);
    if (IsExtensionWithPermissionOrSuggestInConsole(
            APIPermission::kGeolocation, extension,
            web_contents->GetRenderViewHost())) {
      // Make sure the extension is in the calling process.
      if (extensions::ProcessMap::Get(profile_)->Contains(
              extension->id(), request_id.render_process_id())) {
        *permission_set = true;
        *new_permission = true;
        return true;
      }
    }
  }

  if (extensions::GetViewType(web_contents) !=
      extensions::VIEW_TYPE_TAB_CONTENTS) {
    // The tab may have gone away, or the request may not be from a tab at all.
    // TODO(mpcomplete): the request could be from a background page or
    // extension popup (web_contents will have a different ViewType). But why do
    // we care? Shouldn't we still put an infobar up in the current tab?
    LOG(WARNING) << "Attempt to use geolocation tabless renderer: "
                 << request_id.ToString()
                 << " (can't prompt user without a visible tab)";
    *permission_set = true;
    *new_permission = false;
    return true;
  }
#endif  // defined(ENABLE_EXTENSIONS)
  return false;
}

bool GeolocationPermissionContextExtensions::CancelPermissionRequest(
    content::WebContents* web_contents,
    int bridge_id) {
#if defined(ENABLE_EXTENSIONS)
  WebViewPermissionHelper* web_view_permission_helper =
      web_contents ? WebViewPermissionHelper::FromWebContents(web_contents)
                   : NULL;
  if (web_view_permission_helper) {
    web_view_permission_helper->CancelGeolocationPermissionRequest(bridge_id);
    return true;
  }
#endif  // defined(ENABLE_EXTENSIONS)
  return false;
}
