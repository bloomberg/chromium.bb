// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_API_H_
#define CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_API_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/settings/settings_namespace.h"
#include "chrome/browser/extensions/settings/settings_observer.h"
#include "chrome/browser/value_store/value_store.h"

namespace extensions {

// Superclass of all settings functions.
//
// NOTE: these all have "*SettingsFunction" names left over from when the API
// was called the "Settings API" (now "Storage API").
// TODO(kalman): Rename these functions, and all files under
// chrome/browser/extensions/settings.
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

class GetSettingsFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("storage.get");

 protected:
  virtual ~GetSettingsFunction() {}

  // SettingsFunction:
  virtual bool RunWithStorage(ValueStore* storage) OVERRIDE;
};

class SetSettingsFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("storage.set");

 protected:
  virtual ~SetSettingsFunction() {}

  // SettingsFunction:
  virtual bool RunWithStorage(ValueStore* storage) OVERRIDE;

  // ExtensionFunction:
  virtual void GetQuotaLimitHeuristics(
      QuotaLimitHeuristics* heuristics) const OVERRIDE;
};

class RemoveSettingsFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("storage.remove");

 protected:
  virtual ~RemoveSettingsFunction() {}

  // SettingsFunction:
  virtual bool RunWithStorage(ValueStore* storage) OVERRIDE;

  // ExtensionFunction:
  virtual void GetQuotaLimitHeuristics(
      QuotaLimitHeuristics* heuristics) const OVERRIDE;
};

class ClearSettingsFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("storage.clear");

 protected:
  virtual ~ClearSettingsFunction() {}

  // SettingsFunction:
  virtual bool RunWithStorage(ValueStore* storage) OVERRIDE;

  // ExtensionFunction:
  virtual void GetQuotaLimitHeuristics(
      QuotaLimitHeuristics* heuristics) const OVERRIDE;
};

class GetBytesInUseSettingsFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("storage.getBytesInUse");

 protected:
  virtual ~GetBytesInUseSettingsFunction() {}

  // SettingsFunction:
  virtual bool RunWithStorage(ValueStore* storage) OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_API_H_
