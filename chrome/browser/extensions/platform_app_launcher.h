// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PLATFORM_APP_LAUNCHER_H_
#define CHROME_BROWSER_EXTENSIONS_PLATFORM_APP_LAUNCHER_H_
#pragma once

class CommandLine;
class Profile;

namespace webkit_glue {
struct WebIntentData;
}

namespace extensions {

class Extension;

// Launches the platform app |extension|. Creates appropriate launch data for
// the |command_line| fields present. |extension| and |profile| must not be
// NULL. A NULL |command_line| means there is no launch data.
void LaunchPlatformApp(Profile* profile,
                       const Extension* extension,
                       const CommandLine* command_line);

// Launches the platform app |extension| with the supplied web intent. Creates
// appropriate launch data for the |web_intent_data| field present. |extension|
// and |profile| must not be NULL.
void LaunchPlatformAppWithWebIntent(
    Profile* profile,
    const Extension* extension,
    const webkit_glue::WebIntentData& web_intent_data);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PLATFORM_APP_LAUNCHER_H_
