// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_resource_request_policy.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/common/chrome_switches.h"
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

  const Extension* extension =
      loaded_extensions->GetExtensionOrAppByURL(ExtensionURLInfo(resource_url));
  if (!extension) {
    // Allow the load in the case of a non-existent extension. We'll just get a
    // 404 from the browser process.
    return true;
  }

  // Disallow loading of packaged resources for hosted apps. We don't allow
  // hybrid hosted/packaged apps. The one exception is access to icons, since
  // some extensions want to be able to do things like create their own
  // launchers.
  std::string resource_root_relative_path =
      resource_url.path().empty() ? "" : resource_url.path().substr(1);
  if (extension->is_hosted_app() &&
      !extension->icons().ContainsPath(resource_root_relative_path)) {
    LOG(ERROR) << "Denying load of " << resource_url.spec() << " from "
               << "hosted app.";
    return false;
  }

  // Disallow loading of extension resources which are not explicitely listed
  // as web accessible if the manifest version is 2 or greater.

  // Exceptions are:
  // - empty origin (needed for some edge cases when we have empty origins)
  // - chrome-extension:// (for legacy reasons -- some extensions interop)
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableExtensionsResourceWhitelist) &&
      !frame_url.is_empty() &&
      !frame_url.SchemeIs(chrome::kExtensionScheme) &&
      !extension->IsResourceWebAccessible(resource_url.path())) {
    LOG(ERROR) << "Denying load of " << resource_url.spec() << " which "
               << "is not a web accessible resource.";
    return false;
  }

  return true;
}

ExtensionResourceRequestPolicy::ExtensionResourceRequestPolicy() {
}
