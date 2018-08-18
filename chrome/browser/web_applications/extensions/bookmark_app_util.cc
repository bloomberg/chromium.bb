// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"

#include "base/values.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/url_pattern_set.h"
#include "url/gurl.h"

namespace extensions {
namespace {

// A preference used to indicate that a bookmark apps is fully locally installed
// on this machine. The default value (i.e. if the pref is not set) is to be
// fully locally installed, so that hosted apps or bookmark apps created /
// synced before this pref existed will be treated as locally installed.
const char kPrefLocallyInstalled[] = "locallyInstalled";

}  // namespace

void SetBookmarkAppIsLocallyInstalled(content::BrowserContext* context,
                                      const Extension* extension,
                                      bool is_locally_installed) {
  ExtensionPrefs::Get(context)->UpdateExtensionPref(
      extension->id(), kPrefLocallyInstalled,
      std::make_unique<base::Value>(is_locally_installed));
}

bool BookmarkAppIsLocallyInstalled(content::BrowserContext* context,
                                   const Extension* extension) {
  bool locally_installed;
  if (ExtensionPrefs::Get(context)->ReadPrefAsBoolean(
          extension->id(), kPrefLocallyInstalled, &locally_installed)) {
    return locally_installed;
  }

  return true;
}

bool BookmarkOrHostedAppInstalled(content::BrowserContext* browser_context,
                                  const GURL& url) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context);
  const ExtensionSet& extensions = registry->enabled_extensions();

  // Iterate through the extensions and extract the LaunchWebUrl (bookmark apps)
  // or check the web extent (hosted apps).
  for (const scoped_refptr<const Extension>& extension : extensions) {
    if (!extension->is_hosted_app())
      continue;

    if (!BookmarkAppIsLocallyInstalled(browser_context, extension.get()))
      continue;

    if (extension->web_extent().MatchesURL(url) ||
        AppLaunchInfo::GetLaunchWebURL(extension.get()) == url) {
      return true;
    }
  }
  return false;
}

}  // namespace extensions
