// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_RUNTIME_RUNTIME_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_RUNTIME_RUNTIME_API_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"

class Extension;
class ExtensionHost;
class Profile;

namespace extensions {

class RuntimeEventRouter {
 public:
  // Dispatches the onInstalled event to the given extension.
  static void DispatchOnInstalledEvent(Profile* profile,
                                       const std::string& extension_id);
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
