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
  // Extension settings function implementations should do their work here, and
  // either run a StorageResultCallback or fill the function result / call
  // SendResponse themselves.
  // The exception is that implementations can return false to immediately
  // call SendResponse(false), for compliance with EXTENSION_FUNCTION_VALIDATE.
  virtual bool RunWithStorageImpl(ExtensionSettingsStorage* storage) = 0;

  virtual bool RunImpl() OVERRIDE;

  // Callback from all storage methods (Get/Set/Remove/Clear) which sets the
  // appropriate fields of the extension function (result/error) and sends a
  // response.
  // Declared here to access to the protected members of ExtensionFunction.
  class StorageResultCallback : public ExtensionSettingsStorage::Callback {
   public:
    explicit StorageResultCallback(SettingsFunction* settings_function);
    virtual ~StorageResultCallback();
    virtual void OnSuccess(DictionaryValue* settings) OVERRIDE;
    virtual void OnFailure(const std::string& message) OVERRIDE;

   private:
    scoped_refptr<SettingsFunction> settings_function_;
  };

 private:
  // Callback method from GetStorage(); delegates to RunWithStorageImpl.
  void RunWithStorage(ExtensionSettingsStorage* storage);
};

class GetSettingsFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.settings.get");

 protected:
  virtual bool RunWithStorageImpl(ExtensionSettingsStorage* storage) OVERRIDE;
};

class SetSettingsFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.settings.set");

 protected:
  virtual bool RunWithStorageImpl(ExtensionSettingsStorage* storage) OVERRIDE;
};

class RemoveSettingsFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.settings.remove");

 protected:
  virtual bool RunWithStorageImpl(ExtensionSettingsStorage* storage) OVERRIDE;
};

class ClearSettingsFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.settings.clear");

 protected:
  virtual bool RunWithStorageImpl(ExtensionSettingsStorage* storage) OVERRIDE;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SETTINGS_API_H_
