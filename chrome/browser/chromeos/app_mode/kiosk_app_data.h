// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_DATA_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_DATA_H_

#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/webstore_data_fetcher_delegate.h"
#include "ui/gfx/image/image_skia.h"

class Profile;

namespace base {
class RefCountedString;
}

namespace extensions {
class Extension;
class WebstoreDataFetcher;
}

namespace gfx {
class Image;
}

namespace net {
class URLRequestContextGetter;
}

namespace chromeos {

class KioskAppDataDelegate;

// Fetches an app's web store data and manages the cached info such as name
// and icon.
class KioskAppData : public base::SupportsWeakPtr<KioskAppData>,
                     public extensions::WebstoreDataFetcherDelegate {
 public:
  enum Status {
    STATUS_INIT,     // Data initialized with app id.
    STATUS_LOADING,  // Loading data from cache or web store.
    STATUS_LOADED,   // Data loaded.
    STATUS_ERROR,    // Failed to load data.
  };

  KioskAppData(KioskAppDataDelegate* delegate,
               const std::string& app_id,
               const std::string& user_id);
  virtual ~KioskAppData();

  // Loads app data from cache. If there is no cached data, fetches it
  // from web store.
  void Load();

  // Clears locally cached data.
  void ClearCache();

  // Loads app data from the app installed in the given profile.
  void LoadFromInstalledApp(Profile* profile, const extensions::Extension* app);

  // Returns true if web store data fetching is in progress.
  bool IsLoading() const;

  const std::string& app_id() const { return app_id_; }
  const std::string& user_id() const { return user_id_; }
  const std::string& name() const { return name_; }
  const gfx::ImageSkia& icon() const { return icon_; }
  const base::RefCountedString* raw_icon() const {
    return raw_icon_.get();
  }
  Status status() const { return status_; }

 private:
  class IconLoader;
  class WebstoreDataParser;

  void SetStatus(Status status);

  // Returns URLRequestContextGetter to use for fetching web store data.
  net::URLRequestContextGetter* GetRequestContextGetter();

  // Loads the locally cached data. Return false if there is none.
  bool LoadFromCache();

  // Sets the cached data.
  void SetCache(const std::string& name, const base::FilePath& icon_path);

  // Helper to set the cached data using a SkBitmap icon.
  void SetCache(const std::string& name, const SkBitmap& icon);

  // Callback for extensions::ImageLoader.
  void OnExtensionIconLoaded(const gfx::Image& icon);

  // Callbacks for IconLoader.
  void OnIconLoadSuccess(const scoped_refptr<base::RefCountedString>& raw_icon,
                         const gfx::ImageSkia& icon);
  void OnIconLoadFailure();

  // Callbacks for WebstoreDataParser
  void OnWebstoreParseSuccess(const SkBitmap& icon);
  void OnWebstoreParseFailure();

  // Starts to fetch data from web store.
  void StartFetch();

  // extensions::WebstoreDataFetcherDelegate overrides:
  virtual void OnWebstoreRequestFailure() OVERRIDE;
  virtual void OnWebstoreResponseParseSuccess(
      scoped_ptr<base::DictionaryValue> webstore_data) OVERRIDE;
  virtual void OnWebstoreResponseParseFailure(
      const std::string& error) OVERRIDE;

  // Helper function for testing for the existence of |key| in
  // |response|. Passes |key|'s content via |value| and returns
  // true when |key| is present.
  bool CheckResponseKeyValue(const base::DictionaryValue* response,
                             const char* key,
                             std::string* value);

  KioskAppDataDelegate* delegate_;  // not owned.
  Status status_;

  std::string app_id_;
  std::string user_id_;
  std::string name_;
  gfx::ImageSkia icon_;
  scoped_refptr<base::RefCountedString> raw_icon_;

  scoped_ptr<extensions::WebstoreDataFetcher> webstore_fetcher_;
  base::FilePath icon_path_;

  DISALLOW_COPY_AND_ASSIGN(KioskAppData);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_DATA_H_
