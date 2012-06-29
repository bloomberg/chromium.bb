// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "chrome/browser/android/chrome_startup_flags.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/common/chrome_switches.h"

namespace {

void SetCommandLineSwitch(const std::string& switch_string) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switch_string))
    command_line->AppendSwitch(switch_string);
}

void SetCommandLineSwitchASCII(const std::string& switch_string,
                               const std::string& value) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switch_string))
    command_line->AppendSwitchASCII(switch_string, value);
}

bool IsTabletUi() {
  NOTIMPLEMENTED() << "TODO(yfriedman): Upstream this";
  return false;
}

}  // namespace

void SetChromeSpecificCommandLineFlags() {
  // Always enable SPDY.
  SetCommandLineSwitch(switches::kEnableNpn);

  // Turn on autofill.
  SetCommandLineSwitch(switches::kExternalAutofillPopup);

  // Turn on autologin.
  SetCommandLineSwitch(switches::kEnableAutologin);

  // Tablet UI switch (used for using correct version of NTP HTML).
  if (IsTabletUi())
    SetCommandLineSwitch(switches::kTabletUi);

  // Enable prerender for the omnibox.
  SetCommandLineSwitchASCII(
      switches::kPrerenderMode, switches::kPrerenderModeSwitchValueEnabled);
  SetCommandLineSwitchASCII(
      switches::kPrerenderFromOmnibox,
      switches::kPrerenderFromOmniboxSwitchValueEnabled);
}
