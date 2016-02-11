// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/app_launch_params.h"

#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "ui/base/window_open_disposition.h"

using extensions::ExtensionPrefs;

AppLaunchParams::AppLaunchParams(Profile* profile,
                                 const extensions::Extension* extension,
                                 extensions::LaunchContainer container,
                                 WindowOpenDisposition disposition,
                                 extensions::AppLaunchSource source)
    : profile(profile),
      extension_id(extension ? extension->id() : std::string()),
      container(container),
      disposition(disposition),
      override_url(),
      override_bounds(),
      command_line(base::CommandLine::NO_PROGRAM),
      source(source) {
}

AppLaunchParams::AppLaunchParams(Profile* profile,
                                 const extensions::Extension* extension,
                                 WindowOpenDisposition raw_disposition,
                                 extensions::AppLaunchSource source)
    : profile(profile),
      extension_id(extension ? extension->id() : std::string()),
      container(extensions::LAUNCH_CONTAINER_NONE),
      override_url(),
      override_bounds(),
      command_line(base::CommandLine::NO_PROGRAM),
      source(source) {
  if (raw_disposition == NEW_FOREGROUND_TAB ||
      raw_disposition == NEW_BACKGROUND_TAB) {
    container = extensions::LAUNCH_CONTAINER_TAB;
    disposition = raw_disposition;
  } else if (raw_disposition == NEW_WINDOW) {
    container = extensions::LAUNCH_CONTAINER_WINDOW;
    disposition = raw_disposition;
  } else {
    // Look at preference to find the right launch container.  If no preference
    // is set, launch as a regular tab.
    container =
        extensions::GetLaunchContainer(ExtensionPrefs::Get(profile), extension);
    disposition = NEW_FOREGROUND_TAB;
  }
}

AppLaunchParams::~AppLaunchParams() {
}
