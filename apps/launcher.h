// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_LAUNCHER_H_
#define APPS_LAUNCHER_H_

#include <string>
#include <vector>

class GURL;
class Profile;

namespace base {
class CommandLine;
class FilePath;
}

namespace extensions {
class Extension;
}

namespace apps {

// Launches the platform app |extension|. Creates appropriate launch data for
// the |command_line| fields present. |extension| and |profile| must not be
// NULL. An empty |command_line| means there is no launch data. If non-empty,
// |current_directory| is used to expand any relative paths on the command line.
void LaunchPlatformAppWithCommandLine(Profile* profile,
                                      const extensions::Extension* extension,
                                      const base::CommandLine& command_line,
                                      const base::FilePath& current_directory);

// Launches the platform app |extension| by issuing an onLaunched event
// with the contents of |file_path| available through the launch data.
void LaunchPlatformAppWithPath(Profile* profile,
                               const extensions::Extension* extension,
                               const base::FilePath& file_path);

// Launches the platform app |extension| with no launch data.
void LaunchPlatformApp(Profile* profile,
                       const extensions::Extension* extension);

// Launches the platform app |extension| with |handler_id| and the contents of
// |file_paths| available through the launch data. |handler_id| corresponds to
// the id of the file_handlers item in the manifest that resulted in a match
// that triggered this launch.
void LaunchPlatformAppWithFileHandler(
    Profile* profile,
    const extensions::Extension* extension,
    const std::string& handler_id,
    const std::vector<base::FilePath>& file_paths);

// Launches the platform app |extension| with |handler_id|, |url| and
// |referrer_url| available through the launch data. |handler_id| corresponds to
// the id of the file_handlers item in the manifest that resulted in a match
// that triggered this launch.
void LaunchPlatformAppWithUrl(Profile* profile,
                              const extensions::Extension* extension,
                              const std::string& handler_id,
                              const GURL& url,
                              const GURL& referrer_url);

void RestartPlatformApp(Profile* profile,
                        const extensions::Extension* extension);

}  // namespace apps

#endif  // APPS_LAUNCHER_H_
