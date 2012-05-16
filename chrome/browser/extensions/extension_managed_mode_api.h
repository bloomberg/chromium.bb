// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions Managed Mode API relevant classes to realize
// the API as specified in the extension API JSON.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGED_MODE_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGED_MODE_API_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_observer.h"

class Profile;

class ExtensionManagedModeEventRouter : public content::NotificationObserver {
 public:
  explicit ExtensionManagedModeEventRouter(Profile* profile);
  virtual ~ExtensionManagedModeEventRouter();

  void Init();

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  PrefChangeRegistrar registrar_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionManagedModeEventRouter);
};

class GetManagedModeFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("managedModePrivate.get")

 protected:
  virtual ~GetManagedModeFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class EnterManagedModeFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("managedModePrivate.enter")

 protected:
  virtual ~EnterManagedModeFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  // Called when we have either successfully entered managed mode or failed.
  void SendResult(bool success);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGED_MODE_API_H_
