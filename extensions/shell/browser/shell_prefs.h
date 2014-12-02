// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_PREFS_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_PREFS_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

class PrefService;

namespace content {
class BrowserContext;
}

namespace extensions {

// Support for PrefService initialization and management.
class ShellPrefs {
 public:
  // Creates a pref service that loads user prefs.
  static scoped_ptr<PrefService> CreatePrefService(
      content::BrowserContext* browser_context);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ShellPrefs);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_PREFS_H_
