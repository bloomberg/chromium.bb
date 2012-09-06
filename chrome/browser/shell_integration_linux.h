// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHELL_INTEGRATION_LINUX_H_
#define CHROME_BROWSER_SHELL_INTEGRATION_LINUX_H_

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
FilePath GetWebShortcutFilename(const GURL& url);

// Returns filename for .desktop file based on |profile_path| and
// |extension_id|, sanitized for security.
FilePath GetExtensionShortcutFilename(const FilePath& profile_path,
                                      const std::string& extension_id);

// Returns contents for .desktop file based on |template_contents|, |url|
// and |title|. The |template_contents| should be contents of .desktop file
// used to launch Chrome.
std::string GetDesktopFileContents(const std::string& template_contents,
                                   const std::string& app_name,
                                   const GURL& url,
                                   const std::string& extension_id,
                                   const FilePath& extension_path,
                                   const string16& title,
                                   const std::string& icon_name,
                                   const FilePath& profile_path);


// Create shortcuts on the desktop or in the application menu (as specified by
// |shortcut_info|), for the web page or extension in |shortcut_info|. Use the
// shortcut template contained in |shortcut_template|.
// For extensions, duplicate shortcuts are avoided, so if a requested shortcut
// already exists it is deleted first.
bool CreateDesktopShortcut(const ShellIntegration::ShortcutInfo& shortcut_info,
                           const std::string& shortcut_template);

// Delete any desktop shortcuts on desktop or in the application menu that have
// been added for the extension with |extension_id| in |profile_path|.
void DeleteDesktopShortcuts(const FilePath& profile_path,
                            const std::string& extension_id);

}  // namespace ShellIntegrationLinux

#endif  // CHROME_BROWSER_SHELL_INTEGRATION_LINUX_H_
