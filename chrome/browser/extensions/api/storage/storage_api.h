// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STORAGE_STORAGE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_STORAGE_STORAGE_API_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/storage/settings_namespace.h"
#include "chrome/browser/extensions/api/storage/settings_observer.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/value_store/value_store.h"

namespace extensions {

// Superclass of all settings functions.
class SettingsFunction : public AsyncExtensionFunction {
 protected:
  SettingsFunction();
  virtual ~SettingsFunction();

  // ExtensionFunction:
  virtual bool ShouldSkipQuotaLimiting() const OVERRIDE;
  virtual bool RunImpl() OVERRIDE;

  // Extension settings function implementations should do their work here.
  // The SettingsFrontend makes sure this is posted to the appropriate thread.
  // Implementations should fill in args themselves, though (like RunImpl)
  // may return false to imply failure.
  virtual bool RunWithStorage(ValueStore* storage) = 0;

  // Sets error_ or result_ depending on the value of a storage ReadResult, and
  // returns whether the result implies success (i.e. !error).
  bool UseReadResult(ValueStore::ReadResult result);

  // Sets error_ depending on the value of a storage WriteResult, sends a
  // change notification if needed, and returns whether the result implies
  // success (i.e. !error).
  bool UseWriteResult(ValueStore::WriteResult result);

 private:
  // Called via PostTask from RunImpl.  Calls RunWithStorage and then
  // SendResponse with its success value.
  void AsyncRunWithStorage(ValueStore* storage);

  // The settings namespace the call was for.  For example, SYNC if the API
  // call was chrome.settings.experimental.sync..., LOCAL if .local, etc.
  settings_namespace::Namespace settings_namespace_;

  // Observers, cached so that it's only grabbed from the UI thread.
  scoped_refptr<SettingsObserverList> observers_;
};

class StorageGetFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("storage.get");

 protected:
  virtual ~StorageGetFunction() {}

  // SettingsFunction:
  virtual bool RunWithStorage(ValueStore* storage) OVERRIDE;
};

class StorageSetFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("storage.set");

 protected:
  virtual ~StorageSetFunction() {}

  // SettingsFunction:
  virtual bool RunWithStorage(ValueStore* storage) OVERRIDE;

  // ExtensionFunction:
  virtual void GetQuotaLimitHeuristics(
      QuotaLimitHeuristics* heuristics) const OVERRIDE;
};

class StorageRemoveFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("storage.remove");

 protected:
  virtual ~StorageRemoveFunction() {}

  // SettingsFunction:
  virtual bool RunWithStorage(ValueStore* storage) OVERRIDE;

  // ExtensionFunction:
  virtual void GetQuotaLimitHeuristics(
      QuotaLimitHeuristics* heuristics) const OVERRIDE;
};

class StorageClearFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("storage.clear");

 protected:
  virtual ~StorageClearFunction() {}

  // SettingsFunction:
  virtual bool RunWithStorage(ValueStore* storage) OVERRIDE;

  // ExtensionFunction:
  virtual void GetQuotaLimitHeuristics(
      QuotaLimitHeuristics* heuristics) const OVERRIDE;
};

class StorageGetBytesInUseFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("storage.getBytesInUse");

 protected:
  virtual ~StorageGetBytesInUseFunction() {}

  // SettingsFunction:
  virtual bool RunWithStorage(ValueStore* storage) OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STORAGE_STORAGE_API_H_
