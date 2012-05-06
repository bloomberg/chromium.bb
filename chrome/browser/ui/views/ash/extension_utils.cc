// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/extension_utils.h"

#include "chrome/browser/event_disposition.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension.h"
#include "googleurl/src/gurl.h"

namespace extension_utils {

// Opens an extension.  |event_flags| holds the flags of the event
// which invokes this extension.
void OpenExtension(Profile* profile,
                   const Extension* extension,
                   int event_flags) {
  DCHECK(profile);
  DCHECK(extension);

  WindowOpenDisposition disposition =
      browser::DispositionFromEventFlags(event_flags);

  GURL url;
  if (extension->id() == extension_misc::kWebStoreAppId)
    url = extension->GetFullLaunchURL();

  if (disposition == NEW_FOREGROUND_TAB || disposition == NEW_BACKGROUND_TAB) {
    // Opens in a tab.
    Browser::OpenApplication(
        profile, extension, extension_misc::LAUNCH_TAB, url, disposition);
  } else if (disposition == NEW_WINDOW) {
    // Force a new window open.
    Browser::OpenApplication(
        profile, extension, extension_misc::LAUNCH_WINDOW, url,
        disposition);
  } else {
    // Look at preference to find the right launch container.  If no preference
    // is set, launch as a regular tab.
    extension_misc::LaunchContainer launch_container =
        profile->GetExtensionService()->extension_prefs()->GetLaunchContainer(
            extension, ExtensionPrefs::LAUNCH_DEFAULT);

    Browser::OpenApplication(
        profile, extension, launch_container, GURL(url),
        NEW_FOREGROUND_TAB);
  }
}

}  // namespace extension_utils
