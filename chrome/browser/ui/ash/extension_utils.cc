// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/extension_utils.h"

#include "chrome/browser/event_disposition.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/extensions/extension.h"
#include "googleurl/src/gurl.h"

namespace extension_utils {

// Opens an extension.  |event_flags| holds the flags of the event
// which invokes this extension.
void OpenExtension(Profile* profile,
                   const extensions::Extension* extension,
                   int event_flags) {
  DCHECK(profile);
  DCHECK(extension);

  WindowOpenDisposition disposition =
      chrome::DispositionFromEventFlags(event_flags);
  extension_misc::LaunchContainer container;

  if (disposition == NEW_FOREGROUND_TAB || disposition == NEW_BACKGROUND_TAB) {
    container = extension_misc::LAUNCH_TAB;
  } else if (disposition == NEW_WINDOW) {
    container = extension_misc::LAUNCH_WINDOW;
  } else {
    // Look at preference to find the right launch container.  If no preference
    // is set, launch as a regular tab.
    container =
        profile->GetExtensionService()->extension_prefs()->GetLaunchContainer(
            extension, extensions::ExtensionPrefs::LAUNCH_DEFAULT);
    disposition = NEW_FOREGROUND_TAB;
  }

  application_launch::OpenApplication(application_launch::LaunchParams(
          profile, extension, container, disposition));
}

}  // namespace extension_utils
