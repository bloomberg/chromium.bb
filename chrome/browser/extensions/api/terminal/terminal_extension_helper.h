// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TERMINAL_TERMINAL_EXTENSION_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_API_TERMINAL_TERMINAL_EXTENSION_HELPER_H_
#pragma once

#include <string>

#include "googleurl/src/gurl.h"

class Profile;

class TerminalExtensionHelper {
 public:
  // Checks if the extension whose id is |extension_id| is allowed to access
  // terminalPrivate API.
  static bool AllowAccessToExtension(Profile* profile,
                                     const std::string& extension_id);
  // Returns Hterm extension's entry point for Crosh. If no HTerm extension is
  // installed, returns empty url.
  static GURL GetCroshExtensionURL(Profile* profile);

 private:
  // Returns id of installed HTerm extension (if any), or NULL.
  static const char* GetTerminalExtensionId(Profile* profile);
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_TERMINAL_TERMINAL_EXTENSION_HELPER_H_
