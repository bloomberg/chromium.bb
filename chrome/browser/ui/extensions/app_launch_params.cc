// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/app_launch_params.h"

#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"

using extensions::ExtensionPrefs;

AppLaunchParams::AppLaunchParams(Profile* profile,
                                 const extensions::Extension* extension,
                                 extensions::LaunchContainer container,
                                 WindowOpenDisposition disposition)
    : profile(profile),
      extension_id(extension ? extension->id() : std::string()),
      container(container),
      disposition(disposition),
      desktop_type(chrome::GetActiveDesktop()),
      override_url(),
      override_bounds(),
      command_line(CommandLine::NO_PROGRAM),
      source(extensions::SOURCE_UNTRACKED) {
}

AppLaunchParams::AppLaunchParams(Profile* profile,
                                 const extensions::Extension* extension,
                                 WindowOpenDisposition disposition)
    : profile(profile),
      extension_id(extension ? extension->id() : std::string()),
      container(extensions::LAUNCH_CONTAINER_NONE),
      disposition(disposition),
      desktop_type(chrome::GetActiveDesktop()),
      override_url(),
      override_bounds(),
      command_line(CommandLine::NO_PROGRAM),
      source(extensions::SOURCE_UNTRACKED) {
  // Look up the app preference to find out the right launch container. Default
  // is to launch as a regular tab.
  container =
      extensions::GetLaunchContainer(ExtensionPrefs::Get(profile), extension);
}

AppLaunchParams::AppLaunchParams(Profile* profile,
                                 const extensions::Extension* extension,
                                 int event_flags,
                                 chrome::HostDesktopType desktop_type)
    : profile(profile),
      extension_id(extension ? extension->id() : std::string()),
      container(extensions::LAUNCH_CONTAINER_NONE),
      disposition(ui::DispositionFromEventFlags(event_flags)),
      desktop_type(desktop_type),
      override_url(),
      override_bounds(),
      command_line(CommandLine::NO_PROGRAM),
      source(extensions::SOURCE_UNTRACKED) {
  if (disposition == NEW_FOREGROUND_TAB || disposition == NEW_BACKGROUND_TAB) {
    container = extensions::LAUNCH_CONTAINER_TAB;
  } else if (disposition == NEW_WINDOW) {
    container = extensions::LAUNCH_CONTAINER_WINDOW;
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
