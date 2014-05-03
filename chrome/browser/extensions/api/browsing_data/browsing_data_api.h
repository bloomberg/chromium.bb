// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions BrowsingData API functions, which entail
// clearing browsing data, and clearing the browser's cache (which, let's be
// honest, are the same thing), as specified in the extension API JSON.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BROWSING_DATA_BROWSING_DATA_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_BROWSING_DATA_BROWSING_DATA_API_H_

#include <string>

#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/extensions/chrome_extension_function.h"

class PluginPrefs;

namespace extension_browsing_data_api_constants {

// Parameter name keys.
extern const char kDataRemovalPermittedKey[];
extern const char kDataToRemoveKey[];
extern const char kOptionsKey[];

// Type keys.
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

// Option keys.
extern const char kExtensionsKey[];
extern const char kOriginTypesKey[];
extern const char kProtectedWebKey[];
extern const char kSinceKey[];
extern const char kUnprotectedWebKey[];

// Errors!
extern const char kBadDataTypeDetails[];
extern const char kDeleteProhibitedError[];
extern const char kOneAtATimeError[];

}  // namespace extension_browsing_data_api_constants

class BrowsingDataSettingsFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.settings", BROWSINGDATA_SETTINGS)

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;

 protected:
  virtual ~BrowsingDataSettingsFunction() {}

 private:
  // Sets a boolean value in the |selected_dict| with the |data_type| as a key,
  // indicating whether the data type is both selected and permitted to be
  // removed; and a value in the |permitted_dict| with the |data_type| as a
  // key, indicating only whether the data type is permitted to be removed.
  void SetDetails(base::DictionaryValue* selected_dict,
                  base::DictionaryValue* permitted_dict,
                  const char* data_type,
                  bool is_selected);
};

// This serves as a base class from which the browsing data API removal
// functions will inherit. Each needs to be an observer of BrowsingDataRemover
// events, and each will handle those events in the same way (by calling the
// passed-in callback function).
//
// Each child class must implement GetRemovalMask(), which returns the bitmask
// of data types to remove.
class BrowsingDataRemoverFunction : public ChromeAsyncExtensionFunction,
                                    public BrowsingDataRemover::Observer {
 public:
  // BrowsingDataRemover::Observer interface method.
  virtual void OnBrowsingDataRemoverDone() OVERRIDE;

  // ExtensionFunction:
  virtual bool RunAsync() OVERRIDE;

 protected:
  virtual ~BrowsingDataRemoverFunction() {}

  // Children should override this method to provide the proper removal mask
  // based on the API call they represent.
  virtual int GetRemovalMask() = 0;

 private:
  // Updates the removal bitmask according to whether removing plugin data is
  // supported or not.
  void CheckRemovingPluginDataSupported(
      scoped_refptr<PluginPrefs> plugin_prefs);

  // Parse the developer-provided |origin_types| object into an origin_set_mask
  // that can be used with the BrowsingDataRemover.
  int ParseOriginSetMask(const base::DictionaryValue& options);

  // Called when we're ready to start removing data.
  void StartRemoving();

  base::Time remove_since_;
  int removal_mask_;
  int origin_set_mask_;
};

class BrowsingDataRemoveAppcacheFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeAppcache",
                             BROWSINGDATA_REMOVEAPPCACHE)

 protected:
  virtual ~BrowsingDataRemoveAppcacheFunction() {}

  // BrowsingDataRemoverFunction:
  virtual int GetRemovalMask() OVERRIDE;
};

class BrowsingDataRemoveFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.remove", BROWSINGDATA_REMOVE)

 protected:
  virtual ~BrowsingDataRemoveFunction() {}

  // BrowsingDataRemoverFunction:
  virtual int GetRemovalMask() OVERRIDE;
};

class BrowsingDataRemoveCacheFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeCache",
                             BROWSINGDATA_REMOVECACHE)

 protected:
  virtual ~BrowsingDataRemoveCacheFunction() {}

  // BrowsingDataRemoverFunction:
  virtual int GetRemovalMask() OVERRIDE;
};

class BrowsingDataRemoveCookiesFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeCookies",
                             BROWSINGDATA_REMOVECOOKIES)

 protected:
  virtual ~BrowsingDataRemoveCookiesFunction() {}

  // BrowsingDataRemoverFunction:
  virtual int GetRemovalMask() OVERRIDE;
};

class BrowsingDataRemoveDownloadsFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeDownloads",
                             BROWSINGDATA_REMOVEDOWNLOADS)

 protected:
  virtual ~BrowsingDataRemoveDownloadsFunction() {}

  // BrowsingDataRemoverFunction:
  virtual int GetRemovalMask() OVERRIDE;
};

class BrowsingDataRemoveFileSystemsFunction
    : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeFileSystems",
                             BROWSINGDATA_REMOVEFILESYSTEMS)

 protected:
  virtual ~BrowsingDataRemoveFileSystemsFunction() {}

  // BrowsingDataRemoverFunction:
  virtual int GetRemovalMask() OVERRIDE;
};

class BrowsingDataRemoveFormDataFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeFormData",
                             BROWSINGDATA_REMOVEFORMDATA)

 protected:
  virtual ~BrowsingDataRemoveFormDataFunction() {}

  // BrowsingDataRemoverFunction:
  virtual int GetRemovalMask() OVERRIDE;
};

class BrowsingDataRemoveHistoryFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeHistory",
                             BROWSINGDATA_REMOVEHISTORY)

 protected:
  virtual ~BrowsingDataRemoveHistoryFunction() {}

  // BrowsingDataRemoverFunction:
  virtual int GetRemovalMask() OVERRIDE;
};

class BrowsingDataRemoveIndexedDBFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeIndexedDB",
                             BROWSINGDATA_REMOVEINDEXEDDB)

 protected:
  virtual ~BrowsingDataRemoveIndexedDBFunction() {}

  // BrowsingDataRemoverFunction:
  virtual int GetRemovalMask() OVERRIDE;
};

class BrowsingDataRemoveLocalStorageFunction
    : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeLocalStorage",
                             BROWSINGDATA_REMOVELOCALSTORAGE)

 protected:
  virtual ~BrowsingDataRemoveLocalStorageFunction() {}

  // BrowsingDataRemoverFunction:
  virtual int GetRemovalMask() OVERRIDE;
};

class BrowsingDataRemovePluginDataFunction
    : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removePluginData",
                             BROWSINGDATA_REMOVEPLUGINDATA)

 protected:
  virtual ~BrowsingDataRemovePluginDataFunction() {}

  // BrowsingDataRemoverFunction:
  virtual int GetRemovalMask() OVERRIDE;
};

class BrowsingDataRemovePasswordsFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removePasswords",
                             BROWSINGDATA_REMOVEPASSWORDS)

 protected:
  virtual ~BrowsingDataRemovePasswordsFunction() {}

  // BrowsingDataRemoverFunction:
  virtual int GetRemovalMask() OVERRIDE;
};

class BrowsingDataRemoveWebSQLFunction : public BrowsingDataRemoverFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browsingData.removeWebSQL",
                             BROWSINGDATA_REMOVEWEBSQL)

 protected:
  virtual ~BrowsingDataRemoveWebSQLFunction() {}

  // BrowsingDataRemoverFunction:
  virtual int GetRemovalMask() OVERRIDE;
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_BROWSING_DATA_BROWSING_DATA_API_H_
