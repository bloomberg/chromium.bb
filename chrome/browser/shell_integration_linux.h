// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHELL_INTEGRATION_LINUX_H_
#define CHROME_BROWSER_SHELL_INTEGRATION_LINUX_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "chrome/browser/shell_integration.h"

namespace ShellIntegrationLinux {

bool CreateDesktopShortcutForChromeApp(
    const ShellIntegration::ShortcutInfo& shortcut_info,
    const FilePath& web_app_path,
    const std::string& shortcut_template);

}  // namespace ShellIntegrationLinux

#endif  // CHROME_BROWSER_SHELL_INTEGRATION_LINUX_H_
