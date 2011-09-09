// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_API_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_settings.h"
#include "chrome/browser/extensions/extension_settings_storage.h"

// Superclass of all settings functions.
class SettingsFunction : public AsyncExtensionFunction {
 public:
  // Extension settings function implementations should do their work here.
  // This runs on the FILE thread.
  //
  // Implementations should fill in args themselves, though (like RunImpl)
  // may return false to imply failure.
  virtual bool RunOnFileThreadImpl(ExtensionSettingsStorage* storage) = 0;

 protected:
  virtual bool RunImpl() OVERRIDE;

  // Sets error_ or result_ depending on the value of a storage Result, and
  // returns whether the Result implies success (i.e. !error).
  bool UseResult(const ExtensionSettingsStorage::Result& storage_result);

 private:
  // Callback from GetStorage.
  void RunOnUIThreadWithStorage(ExtensionSettingsStorage* storage);

  // Called from RunOnUIThreadWithStorage.  Runs RunOnFileThreadImpl and sends
  // a response (on the UI thread) with its return value.
  void RunOnFileThreadWithStorage(ExtensionSettingsStorage* storage);
};

class GetSettingsFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.settings.get");

 protected:
  virtual bool RunOnFileThreadImpl(ExtensionSettingsStorage* storage) OVERRIDE;
};

class SetSettingsFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.settings.set");

 protected:
  virtual bool RunOnFileThreadImpl(ExtensionSettingsStorage* storage) OVERRIDE;
};

class RemoveSettingsFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.settings.remove");

 protected:
  virtual bool RunOnFileThreadImpl(ExtensionSettingsStorage* storage) OVERRIDE;
};

class ClearSettingsFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.settings.clear");

 protected:
  virtual bool RunOnFileThreadImpl(ExtensionSettingsStorage* storage) OVERRIDE;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_API_H_
