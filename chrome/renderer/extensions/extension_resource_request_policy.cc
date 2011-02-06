// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_resource_request_policy.h"

#include "base/logging.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "googleurl/src/gurl.h"

// static
bool ExtensionResourceRequestPolicy::CanRequestResource(
    const GURL& resource_url,
    const GURL& frame_url,
    const ExtensionSet* loaded_extensions) {
  CHECK(resource_url.SchemeIs(chrome::kExtensionScheme));

  // chrome:// URLs are always allowed to load chrome-extension:// resources.
  // The app launcher in the NTP uses this feature, as does dev tools.
  if (frame_url.SchemeIs(chrome::kChromeDevToolsScheme) ||
      frame_url.SchemeIs(chrome::kChromeUIScheme))
    return true;

  // Disallow loading of packaged resources for hosted apps. We don't allow
  // hybrid hosted/packaged apps. The one exception is access to icons, since
  // some extensions want to be able to do things like create their own
  // launchers.
  const Extension* extension = loaded_extensions->GetByURL(resource_url);
  std::string resource_root_relative_path =
      resource_url.path().empty() ? "" : resource_url.path().substr(1);
  if (extension && extension->is_hosted_app() &&
      !extension->icons().ContainsPath(resource_root_relative_path)) {
    LOG(ERROR) << "Denying load of " << resource_url.spec() << " from "
               << "hosted app.";
    return false;
  }

  // Otherwise, pages are allowed to load resources from extensions if the
  // extension has host permissions to (and therefore could be running script
  // in, which might need access to the extension resources).
  //
  // Exceptions are:
  // - empty origin (needed for some edge cases when we have empty origins)
  // - chrome-extension:// (for legacy reasons -- some extensions interop)
  // - data: (basic HTML notifications use data URLs internally)
  if (frame_url.is_empty() ||
      frame_url.SchemeIs(chrome::kExtensionScheme) |
      frame_url.SchemeIs(chrome::kDataScheme)) {
    return true;
  } else {
    if (extension->GetEffectiveHostPermissions().ContainsURL(frame_url)) {
      return true;
    } else {
      LOG(ERROR) << "Denying load of " << resource_url.spec() << " from "
                 << frame_url.spec() << " because the extension does not have "
                 << "access to the requesting page.";
      return false;
    }
  }
}

ExtensionResourceRequestPolicy::ExtensionResourceRequestPolicy() {
}
