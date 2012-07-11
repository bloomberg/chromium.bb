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
#include "googleurl/src/gurl.h"

namespace base {
class Environment;
}

namespace ShellIntegrationLinux {

// Returns filename of the desktop shortcut used to launch the browser.
std::string GetDesktopName(base::Environment* env);

bool GetDesktopShortcutTemplate(base::Environment* env,
                                std::string* output);

// Returns filename for .desktop file based on |url|, sanitized for security.
FilePath GetDesktopShortcutFilename(const GURL& url);

// Returns contents for .desktop file based on |template_contents|, |url|
// and |title|. The |template_contents| should be contents of .desktop file
// used to launch Chrome.
std::string GetDesktopFileContents(const std::string& template_contents,
                                   const std::string& app_name,
                                   const GURL& url,
                                   const std::string& extension_id,
                                   const bool is_platform_app,
                                   const FilePath& extension_path,
                                   const string16& title,
                                   const std::string& icon_name,
                                   const FilePath& profile_path);

bool CreateDesktopShortcut(const ShellIntegration::ShortcutInfo& shortcut_info,
                           const std::string& shortcut_template);

}  // namespace ShellIntegrationLinux

#endif  // CHROME_BROWSER_SHELL_INTEGRATION_LINUX_H_
