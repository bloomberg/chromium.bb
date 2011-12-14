// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_process_policy.h"

#include "chrome/common/extensions/extension_set.h"

namespace extensions {

const Extension* GetNonBookmarkAppExtension(
    const ExtensionSet& extensions, const ExtensionURLInfo& url) {
  // Exclude bookmark apps, which do not use the app process model.
  const Extension* extension = extensions.GetExtensionOrAppByURL(url);
  if (extension && extension->from_bookmark())
    extension = NULL;
  return extension;
}

bool CrossesExtensionProcessBoundary(
    const ExtensionSet& extensions,
    const ExtensionURLInfo& old_url,
    const ExtensionURLInfo& new_url) {
  const Extension* old_url_extension = GetNonBookmarkAppExtension(extensions,
                                                                  old_url);
  const Extension* new_url_extension = GetNonBookmarkAppExtension(extensions,
                                                                  new_url);

  // TODO(creis): Temporary workaround for crbug.com/59285: Only return true if
  // we would enter an extension app's extent from a non-app, or if we leave an
  // extension with no web extent.  We avoid swapping processes to exit a hosted
  // app for now, since we do not yet support postMessage calls from outside the
  // app back into it (e.g., as in Facebook OAuth 2.0).
  bool old_url_is_hosted_app = old_url_extension &&
      !old_url_extension->web_extent().is_empty();
  if (old_url_is_hosted_app)
    return false;

  return old_url_extension != new_url_extension;
}

}  // namespace extensions
