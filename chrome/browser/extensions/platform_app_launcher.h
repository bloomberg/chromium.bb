// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PLATFORM_APP_LAUNCHER_H_
#define CHROME_BROWSER_EXTENSIONS_PLATFORM_APP_LAUNCHER_H_

class CommandLine;
class FilePath;
class Profile;

namespace content {
class WebContents;
class WebIntentsDispatcher;
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
                       const FilePath& current_directory);

// Launches the platform app |extension| with the contents of |file_path|
// available through the launch data.
void LaunchPlatformAppWithPath(Profile* profile,
                               const Extension* extension,
                               const FilePath& file_path);

// Launches the platform app |extension| with the supplied web intent. Creates
// appropriate launch data for the |web_intent_data| field present. |extension|
// and |profile| must not be NULL.
void LaunchPlatformAppWithWebIntent(
    Profile* profile,
    const Extension* extension,
    content::WebIntentsDispatcher* intents_dispatcher,
    content::WebContents* source);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PLATFORM_APP_LAUNCHER_H_
