// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_API_H_
#define CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_API_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/settings/settings_backend.h"
#include "chrome/browser/extensions/settings/settings_namespace.h"
#include "chrome/browser/extensions/settings/settings_storage.h"

namespace extensions {

// Superclass of all settings functions.
//
// NOTE: these all have "*SettingsFunction" names left over from when the API
// was called the "Settings API" (now "Storage API").
// TODO(kalman): Rename these functions, and all files under
// chrome/browser/extensions/settings.
class SettingsFunction : public AsyncExtensionFunction {
 public:
  SettingsFunction();
  virtual ~SettingsFunction();

 protected:
  virtual bool RunImpl() OVERRIDE;

  // Extension settings function implementations should do their work here.
  // This runs on the FILE thread.
  //
  // Implementations should fill in args themselves, though (like RunImpl)
  // may return false to imply failure.
  virtual bool RunWithStorage(SettingsStorage* storage) = 0;

  // Sets error_ or result_ depending on the value of a storage ReadResult, and
  // returns whether the result implies success (i.e. !error).
  bool UseReadResult(const SettingsStorage::ReadResult& result);

  // Sets error_ depending on the value of a storage WriteResult, sends a
  // change notification if needed, and returns whether the result implies
  // success (i.e. !error).
  bool UseWriteResult(const SettingsStorage::WriteResult& result);

 private:
  // Called via PostTask from RunImpl.  Calls RunWithStorage and then
  // SendReponse with its success value.
  void RunWithStorageOnFileThread(SettingsStorage* storage);

  // The settings namespace the call was for.  For example, SYNC if the API
  // call was chrome.settings.experimental.sync..., LOCAL if .local, etc.
  settings_namespace::Namespace settings_namespace_;

  // Observers, cached so that it's only grabbed from the UI thread.
  scoped_refptr<SettingsObserverList> observers_;
};

class GetSettingsFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.storage.get");

 protected:
  virtual bool RunWithStorage(SettingsStorage* storage) OVERRIDE;
};

class SetSettingsFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.storage.set");

 protected:
  virtual bool RunWithStorage(SettingsStorage* storage) OVERRIDE;

  virtual void GetQuotaLimitHeuristics(
      QuotaLimitHeuristics* heuristics) const OVERRIDE;
};

class RemoveSettingsFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.storage.remove");

 protected:
  virtual bool RunWithStorage(SettingsStorage* storage) OVERRIDE;

  virtual void GetQuotaLimitHeuristics(
      QuotaLimitHeuristics* heuristics) const OVERRIDE;
};

class ClearSettingsFunction : public SettingsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.storage.clear");

 protected:
  virtual bool RunWithStorage(SettingsStorage* storage) OVERRIDE;

  virtual void GetQuotaLimitHeuristics(
      QuotaLimitHeuristics* heuristics) const OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SETTINGS_SETTINGS_API_H_
