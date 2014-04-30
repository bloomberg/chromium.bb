// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FIRST_RUN_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FIRST_RUN_PRIVATE_API_H_

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/common/extensions/api/first_run_private.h"

class FirstRunPrivateGetLocalizedStringsFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("firstRunPrivate.getLocalizedStrings",
                             FIRSTRUNPRIVATE_GETLOCALIZEDSTRINGS)

 protected:
  virtual ~FirstRunPrivateGetLocalizedStringsFunction() {}

  // SyncExtensionFunction overrides.
  virtual bool RunSync() OVERRIDE;
};

class FirstRunPrivateLaunchTutorialFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("firstRunPrivate.launchTutorial",
                             FIRSTRUNPRIVATE_LAUNCHTUTORIAL)

 protected:
  virtual ~FirstRunPrivateLaunchTutorialFunction() {}

  // SyncExtensionFunction overrides.
  virtual bool RunSync() OVERRIDE;
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FIRST_RUN_PRIVATE_API_H_
