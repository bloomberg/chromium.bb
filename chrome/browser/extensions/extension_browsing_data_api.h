// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions BrowsingData API functions, which entail
// clearing browsing data, and clearing the browser's cache (which, let's be
// honest, are the same thing), as specified in the extension API JSON.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSING_DATA_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSING_DATA_API_H_
#pragma once

#include <string>

#include "chrome/browser/browsing_data_remover.h"
#include "chrome/browser/extensions/extension_function.h"

class PluginPrefs;

namespace extension_browsing_data_api_constants {

// Keys.
extern const char kAppCacheKey[];
extern const char kCacheKey[];
extern const char kCookiesKey[];
extern const char kDownloadsKey[];
extern const char kFileSystemsKey[];
extern const char kFormDataKey[];
extern const char kHistoryKey[];
extern const char kIndexedDBKey[];
extern const char kPluginDataKey[];
extern const char kLocalStorageKey[];
extern const char kPasswordsKey[];
extern const char kWebSQLKey[];

// Errors!
extern const char kOneAtATimeError[];

}  // namespace extension_browsing_data_api_constants

// This serves as a base class from which the browsing data API functions will
// inherit. Each needs to be an observer of BrowsingDataRemover events, and each
// will handle those events in the same way (by calling the passed-in callback
// function).
//
// Each child class must implement GetRemovalMask(), which returns the bitmask
// of data types to remove.
class BrowsingDataExtensionFunction : public AsyncExtensionFunction,
                                      public BrowsingDataRemover::Observer {
 public:
  // BrowsingDataRemover::Observer interface method.
  virtual void OnBrowsingDataRemoverDone() OVERRIDE;

  // AsyncExtensionFunction interface method.
  virtual bool RunImpl() OVERRIDE;

 protected:
  // Children should override this method to provide the proper removal mask
  // based on the API call they represent.
  virtual int GetRemovalMask() const = 0;

 private:
  // Updates the removal bitmask according to whether removing plugin data is
  // supported or not.
  void CheckRemovingPluginDataSupported(
      scoped_refptr<PluginPrefs> plugin_prefs);

  // Called when we're ready to start removing data.
  void StartRemoving();

  base::Time remove_since_;
  int removal_mask_;
};

class RemoveAppCacheFunction : public BrowsingDataExtensionFunction {
 public:
  RemoveAppCacheFunction() {}
  virtual ~RemoveAppCacheFunction() {}

 protected:
  // BrowsingDataTypeExtensionFunction interface method.
  virtual int GetRemovalMask() const OVERRIDE;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.browsingData.removeAppcache")
};

class RemoveBrowsingDataFunction : public BrowsingDataExtensionFunction {
 public:
  RemoveBrowsingDataFunction() {}
  virtual ~RemoveBrowsingDataFunction() {}

 protected:
  // BrowsingDataExtensionFunction interface method.
  virtual int GetRemovalMask() const OVERRIDE;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.browsingData.remove")
};

class RemoveCacheFunction : public BrowsingDataExtensionFunction {
 public:
  RemoveCacheFunction() {}
  virtual ~RemoveCacheFunction() {}

 protected:
  // BrowsingDataTypeExtensionFunction interface method.
  virtual int GetRemovalMask() const OVERRIDE;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.browsingData.removeCache")
};

class RemoveCookiesFunction : public BrowsingDataExtensionFunction {
 public:
  RemoveCookiesFunction() {}
  virtual ~RemoveCookiesFunction() {}

 protected:
  // BrowsingDataTypeExtensionFunction interface method.
  virtual int GetRemovalMask() const OVERRIDE;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.browsingData.removeCookies")
};

class RemoveDownloadsFunction : public BrowsingDataExtensionFunction {
 public:
  RemoveDownloadsFunction() {}
  virtual ~RemoveDownloadsFunction() {}

 protected:
  // BrowsingDataTypeExtensionFunction interface method.
  virtual int GetRemovalMask() const OVERRIDE;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.browsingData.removeDownloads")
};

class RemoveFileSystemsFunction : public BrowsingDataExtensionFunction {
 public:
  RemoveFileSystemsFunction() {}
  virtual ~RemoveFileSystemsFunction() {}

 protected:
  // BrowsingDataTypeExtensionFunction interface method.
  virtual int GetRemovalMask() const OVERRIDE;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.browsingData.removeFileSystems")
};

class RemoveFormDataFunction : public BrowsingDataExtensionFunction {
 public:
  RemoveFormDataFunction() {}
  virtual ~RemoveFormDataFunction() {}

 protected:
  // BrowsingDataTypeExtensionFunction interface method.
  virtual int GetRemovalMask() const OVERRIDE;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.browsingData.removeFormData")
};

class RemoveHistoryFunction : public BrowsingDataExtensionFunction {
 public:
  RemoveHistoryFunction() {}
  virtual ~RemoveHistoryFunction() {}

 protected:
  // BrowsingDataTypeExtensionFunction interface method.
  virtual int GetRemovalMask() const OVERRIDE;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.browsingData.removeHistory")
};

class RemoveIndexedDBFunction : public BrowsingDataExtensionFunction {
 public:
  RemoveIndexedDBFunction() {}
  virtual ~RemoveIndexedDBFunction() {}

 protected:
  // BrowsingDataTypeExtensionFunction interface method.
  virtual int GetRemovalMask() const OVERRIDE;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.browsingData.removeIndexedDB")
};

class RemoveLocalStorageFunction : public BrowsingDataExtensionFunction {
 public:
  RemoveLocalStorageFunction() {}
  virtual ~RemoveLocalStorageFunction() {}

 protected:
  // BrowsingDataTypeExtensionFunction interface method.
  virtual int GetRemovalMask() const OVERRIDE;

  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.browsingData.removeLocalStorage")
};

class RemoveOriginBoundCertsFunction : public BrowsingDataExtensionFunction {
 public:
  RemoveOriginBoundCertsFunction() {}
  virtual ~RemoveOriginBoundCertsFunction() {}

 protected:
  // BrowsingDataTypeExtensionFunction interface method.
  virtual int GetRemovalMask() const OVERRIDE;

  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.browsingData.removeOriginBoundCertificates")
};

class RemovePluginDataFunction : public BrowsingDataExtensionFunction {
 public:
  RemovePluginDataFunction() {}
  virtual ~RemovePluginDataFunction() {}

 protected:
  // BrowsingDataTypeExtensionFunction interface method.
  virtual int GetRemovalMask() const OVERRIDE;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.browsingData.removePluginData")
};

class RemovePasswordsFunction : public BrowsingDataExtensionFunction {
 public:
  RemovePasswordsFunction() {}
  virtual ~RemovePasswordsFunction() {}

 protected:
  // BrowsingDataTypeExtensionFunction interface method.
  virtual int GetRemovalMask() const OVERRIDE;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.browsingData.removePasswords")
};

class RemoveWebSQLFunction : public BrowsingDataExtensionFunction {
 public:
  RemoveWebSQLFunction() {}
  virtual ~RemoveWebSQLFunction() {}

 protected:
  // BrowsingDataTypeExtensionFunction interface method.
  virtual int GetRemovalMask() const OVERRIDE;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.browsingData.removeWebSQL")
};
#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSING_DATA_API_H_
