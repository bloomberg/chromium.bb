// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PROXY_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PROXY_API_H_

#include <string>

#include "chrome/browser/extensions/extension_function.h"

class DictionaryValue;

class ProxySettingsFunction : public SyncExtensionFunction {
 public:
  virtual ~ProxySettingsFunction() {}
  virtual bool RunImpl() = 0;
 protected:
  // Takes ownership of |pref_value|.
  void ApplyPreference(
      const char* pref_path, Value* pref_value, bool incognito);
  void RemovePreference(const char* pref_path, bool incognito);
};

class UseCustomProxySettingsFunction : public ProxySettingsFunction {
 public:
  virtual ~UseCustomProxySettingsFunction() {}
  virtual bool RunImpl();

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.proxy.useCustomProxySettings")
};

class RemoveCustomProxySettingsFunction : public ProxySettingsFunction {
 public:
  virtual ~RemoveCustomProxySettingsFunction() {}
  virtual bool RunImpl();

  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.proxy.removeCustomProxySettings")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PROXY_API_H_
