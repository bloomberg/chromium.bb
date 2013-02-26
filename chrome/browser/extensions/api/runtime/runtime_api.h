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

  // Dispatches the onUpdateAvailable event to the given extension.
  static void DispatchOnUpdateAvailableEvent(
      Profile* profile,
      const std::string& extension_id,
      const base::DictionaryValue* manifest);

  // Dispatches the onBrowserUpdateAvailable event to all extensions.
  static void DispatchOnBrowserUpdateAvailableEvent(Profile* profile);
};

class RuntimeGetBackgroundPageFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.getBackgroundPage",
                             RUNTIME_GETBACKGROUNDPAGE)

 protected:
  virtual ~RuntimeGetBackgroundPageFunction() {}
  virtual bool RunImpl() OVERRIDE;

 private:
  void OnPageLoaded(ExtensionHost*);
};

class RuntimeReloadFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.reload", RUNTIME_RELOAD)

 protected:
  virtual ~RuntimeReloadFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class RuntimeRequestUpdateCheckFunction : public AsyncExtensionFunction,
                                          public content::NotificationObserver {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.requestUpdateCheck",
                             RUNTIME_REQUESTUPDATECHECK)

  RuntimeRequestUpdateCheckFunction();
 protected:
  virtual ~RuntimeRequestUpdateCheckFunction() {}
  virtual bool RunImpl() OVERRIDE;

  // Implements content::NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;
 private:
  void CheckComplete();
  void ReplyUpdateFound(const std::string& version);

  content::NotificationRegistrar registrar_;
  bool did_reply_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_RUNTIME_RUNTIME_API_H_
