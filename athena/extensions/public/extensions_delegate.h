// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_EXTENSIONS_PUBLIC_EXTENSIONS_DELEGATE_H_
#define ATHENA_EXTENSIONS_PUBLIC_EXTENSIONS_DELEGATE_H_

#include <string>

#include "athena/athena_export.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class ExtensionSet;
}

namespace athena {

// A delegate interface to extension implentation.
class ATHENA_EXPORT ExtensionsDelegate {
 public:
  static ExtensionsDelegate* Get(content::BrowserContext* context);

  // Creates the extension delegate for app shell environment.
  static void CreateExtensionsDelegateForShell(
      content::BrowserContext* context);

  // Creates the extension delegate for test environment.
  static void CreateExtensionsDelegateForTest();

  // Deletes the singleton instance. This must be called in the reverse
  // order of the initialization.
  static void Shutdown();

  ExtensionsDelegate();
  virtual ~ExtensionsDelegate();

  virtual content::BrowserContext* GetBrowserContext() const = 0;

  // Returns the set of extensions that are currently installed.
  virtual const extensions::ExtensionSet& GetInstalledExtensions() = 0;

  // Launch an application specified by |app_id|.
  virtual void LaunchApp(const std::string& app_id) = 0;
};

}  // namespace athena

#endif  // ATHENA_EXTENSIONS_PUBLIC_EXTENSIONS_DELEGATE_H_
