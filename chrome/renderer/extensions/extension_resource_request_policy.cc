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

  const Extension* extension = loaded_extensions->GetByURL(resource_url);
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
  if (extension && extension->is_hosted_app() &&
      !extension->icons().ContainsPath(resource_root_relative_path)) {
    LOG(ERROR) << "Denying load of " << resource_url.spec() << " from "
               << "hosted app.";
    return false;
  }

  return true;
}

ExtensionResourceRequestPolicy::ExtensionResourceRequestPolicy() {
}
