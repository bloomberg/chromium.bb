// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_CHROME_DIRECT_SETTING_H__
#define CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_CHROME_DIRECT_SETTING_H__

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/chrome_extension_function.h"

class PrefService;

namespace extensions {
namespace chromedirectsetting {

// Base class to host instance method helpers.
class DirectSettingFunctionBase : public ChromeSyncExtensionFunction {
 protected:
  DirectSettingFunctionBase();
  virtual ~DirectSettingFunctionBase();

  // Returns the user pref service.
  PrefService* GetPrefService();

  // Returns true if the caller is a component extension.
  bool IsCalledFromComponentExtension();

 private:
  DISALLOW_COPY_AND_ASSIGN(DirectSettingFunctionBase);
};

class GetDirectSettingFunction : public DirectSettingFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("types.private.ChromeDirectSetting.get",
                             TYPES_PRIVATE_CHROMEDIRECTSETTING_GET)

  GetDirectSettingFunction();

 protected:
  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;

 private:
  virtual ~GetDirectSettingFunction();
  DISALLOW_COPY_AND_ASSIGN(GetDirectSettingFunction);
};

class SetDirectSettingFunction : public DirectSettingFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("types.private.ChromeDirectSetting.set",
                             TYPES_PRIVATE_CHROMEDIRECTSETTING_SET)

  SetDirectSettingFunction();

 protected:
  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;

 private:
  virtual ~SetDirectSettingFunction();
  DISALLOW_COPY_AND_ASSIGN(SetDirectSettingFunction);
};

class ClearDirectSettingFunction : public DirectSettingFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("types.private.ChromeDirectSetting.clear",
                             TYPES_PRIVATE_CHROMEDIRECTSETTING_CLEAR)

  ClearDirectSettingFunction();

 protected:
  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;

 private:
  virtual ~ClearDirectSettingFunction();
  DISALLOW_COPY_AND_ASSIGN(ClearDirectSettingFunction);
};

}  // namespace chromedirectsetting
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_CHROME_DIRECT_SETTING_H__

