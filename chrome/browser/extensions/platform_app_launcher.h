// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PLATFORM_APP_LAUNCHER_H_
#define CHROME_BROWSER_EXTENSIONS_PLATFORM_APP_LAUNCHER_H_

#include <string>

class CommandLine;
class Profile;

namespace base {
class FilePath;
}

namespace content {
class WebContents;
}

namespace extensions {

class Extension;

// Launches the platform app |extension|. Creates appropriate launch data for
// the |command_line| fields present. |extension| and |profile| must not be
// NULL. A NULL |command_line| means there is no launch data. If non-empty,
// |current_directory| is used to expand any relative paths on the command line.
void LaunchPlatformApp(Profile* profile,
                       const Extension* extension,
                       const CommandLine* command_line,
                       const base::FilePath& current_directory);

// Launches the platform app |extension| with the contents of |file_path|
// available through the launch data.
void LaunchPlatformAppWithPath(Profile* profile,
                               const Extension* extension,
                               const base::FilePath& file_path);

// Launches the platform app |extension| with the contents of |file_path|
// available through the launch data.
void LaunchPlatformAppWithFileHandler(Profile* profile,
                                      const Extension* extension,
                                      const std::string& handler_id,
                                      const base::FilePath& file_path);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PLATFORM_APP_LAUNCHER_H_
