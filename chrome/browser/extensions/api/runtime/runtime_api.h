// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_RUNTIME_RUNTIME_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_RUNTIME_RUNTIME_API_H_

#include "chrome/browser/extensions/extension_function.h"

class Profile;
class Version;

namespace extensions {
class Extension;
class ExtensionHost;

class RuntimeEventRouter {
 public:
  // Dispatches the onStartup event to all currently-loaded extensions.
  static void DispatchOnStartupEvent(Profile* profile,
                                     const std::string& extension_id);

  // Dispatches the onInstalled event to the given extension.
  static void DispatchOnInstalledEvent(Profile* profile,
                                       const std::string& extension_id,
                                       const Version& old_version,
                                       bool chrome_updated);
};

class RuntimeGetBackgroundPageFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("runtime.getBackgroundPage");

 protected:
  virtual ~RuntimeGetBackgroundPageFunction() {}
  virtual bool RunImpl() OVERRIDE;

 private:
  void OnPageLoaded(ExtensionHost*);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_RUNTIME_RUNTIME_API_H_
