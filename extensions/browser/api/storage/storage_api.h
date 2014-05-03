// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_STORAGE_STORAGE_API_H_
#define EXTENSIONS_BROWSER_API_STORAGE_STORAGE_API_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "extensions/browser/api/storage/settings_namespace.h"
#include "extensions/browser/api/storage/settings_observer.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/value_store/value_store.h"

namespace extensions {

// Superclass of all settings functions.
class SettingsFunction : public UIThreadExtensionFunction {
 protected:
  SettingsFunction();
  virtual ~SettingsFunction();

  // ExtensionFunction:
  virtual bool ShouldSkipQuotaLimiting() const OVERRIDE;
  virtual ResponseAction Run() OVERRIDE;

  // Extension settings function implementations should do their work here.
  // The StorageFrontend makes sure this is posted to the appropriate thread.
  virtual ResponseValue RunWithStorage(ValueStore* storage) = 0;

  // Handles the |result| of a read function.
  // - If the result succeeded, this will set |result_| and return.
  // - If |result| failed with a ValueStore::CORRUPTION error, this will call
  //   RestoreStorageAndRetry(), and return that result.
  // - If the |result| failed with a different error, this will set |error_|
  //   and return.
  ResponseValue UseReadResult(ValueStore::ReadResult result,
                              ValueStore* storage);

  // Handles the |result| of a write function.
  // - If the result succeeded, this will set |result_| and return.
  // - If |result| failed with a ValueStore::CORRUPTION error, this will call
  //   RestoreStorageAndRetry(), and return that result.
  // - If the |result| failed with a different error, this will set |error_|
  //   and return.
  // This will also send out a change notification, if appropriate.
  ResponseValue UseWriteResult(ValueStore::WriteResult result,
                               ValueStore* storage);

 private:
  // Called via PostTask from Run. Calls RunWithStorage and then
  // SendResponse with its success value.
  void AsyncRunWithStorage(ValueStore* storage);

  // Called if we encounter a ValueStore error. If the error is due to
  // corruption, tries to restore the ValueStore and re-run the API function.
  // If the storage cannot be restored or was due to some other error, then sets
  // error and returns. This also sets the |tried_restoring_storage_| flag to
  // ensure we don't enter a loop.
  ResponseValue HandleError(const ValueStore::Error& error,
                            ValueStore* storage);

  // The settings namespace the call was for.  For example, SYNC if the API
  // call was chrome.settings.experimental.sync..., LOCAL if .local, etc.
  settings_namespace::Namespace settings_namespace_;

  // A flag indicating whether or not we have tried to restore storage. We
  // should only ever try once (per API call) in order to avoid entering a loop.
  bool tried_restoring_storage_;

  // Observers, cached so that it's only grabbed from the UI thread.
  scoped_refptr<SettingsObserverList> observers_;
};

class StorageStorageAreaGetFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("storage.get", STORAGE_GET)

 protected:
  virtual ~StorageStorageAreaGetFunction() {}

  // SettingsFunction:
  virtual ResponseValue RunWithStorage(ValueStore* storage) OVERRIDE;
};

class StorageStorageAreaSetFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("storage.set", STORAGE_SET)

 protected:
  virtual ~StorageStorageAreaSetFunction() {}

  // SettingsFunction:
  virtual ResponseValue RunWithStorage(ValueStore* storage) OVERRIDE;

  // ExtensionFunction:
  virtual void GetQuotaLimitHeuristics(
      QuotaLimitHeuristics* heuristics) const OVERRIDE;
};

class StorageStorageAreaRemoveFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("storage.remove", STORAGE_REMOVE)

 protected:
  virtual ~StorageStorageAreaRemoveFunction() {}

  // SettingsFunction:
  virtual ResponseValue RunWithStorage(ValueStore* storage) OVERRIDE;

  // ExtensionFunction:
  virtual void GetQuotaLimitHeuristics(
      QuotaLimitHeuristics* heuristics) const OVERRIDE;
};

class StorageStorageAreaClearFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("storage.clear", STORAGE_CLEAR)

 protected:
  virtual ~StorageStorageAreaClearFunction() {}

  // SettingsFunction:
  virtual ResponseValue RunWithStorage(ValueStore* storage) OVERRIDE;

  // ExtensionFunction:
  virtual void GetQuotaLimitHeuristics(
      QuotaLimitHeuristics* heuristics) const OVERRIDE;
};

class StorageStorageAreaGetBytesInUseFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("storage.getBytesInUse", STORAGE_GETBYTESINUSE)

 protected:
  virtual ~StorageStorageAreaGetBytesInUseFunction() {}

  // SettingsFunction:
  virtual ResponseValue RunWithStorage(ValueStore* storage) OVERRIDE;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_STORAGE_STORAGE_API_H_
