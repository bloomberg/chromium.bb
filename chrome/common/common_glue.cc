// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/app_switches.h"
#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/plugin/npobject_util.h"
#include "googleurl/src/url_util.h"
#include "ui/base/ui_base_switches.h"
#include "webkit/glue/webkit_glue.h"

namespace webkit_glue {

bool GetExeDirectory(FilePath* path) {
  return PathService::Get(base::DIR_EXE, path);
}

bool GetApplicationDirectory(FilePath* path) {
  return PathService::Get(chrome::DIR_APP, path);
}

bool IsPluginRunningInRendererProcess() {
  return !IsPluginProcess();
}

std::string GetWebKitLocale() {
  // The browser process should have passed the locale to the renderer via the
  // --lang command line flag.  In single process mode, this will return the
  // wrong value.  TODO(tc): Fix this for single process mode.
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  const std::string& lang =
      parsed_command_line.GetSwitchValueASCII(switches::kLang);
  DCHECK(!lang.empty() ||
      (!parsed_command_line.HasSwitch(switches::kRendererProcess) &&
       !parsed_command_line.HasSwitch(switches::kPluginProcess)));
  return lang;
}

string16 GetLocalizedString(int message_id) {
  return l10n_util::GetStringUTF16(message_id);
}

}  // namespace webkit_glue
