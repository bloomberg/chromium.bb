// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_WEBAPPS_REGISTRY_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_WEBAPPS_REGISTRY_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/scoped_vector.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "googleurl/src/gurl.h"

namespace base {
class FilePath;
}

namespace google_apis {
class AppList;
}

namespace drive {

// Data structure that defines WebApp
struct DriveWebAppInfo {
  DriveWebAppInfo(const std::string& app_id,
                  const google_apis::InstalledApp::IconList& app_icons,
                  const google_apis::InstalledApp::IconList& document_icons,
                  const std::string& web_store_id,
                  const string16& app_name,
                  const string16& object_type,
                  bool is_primary_selector);
  ~DriveWebAppInfo();

  // Drive app id
  std::string app_id;
  // Drive application icon URLs for this app, paired with their size (length of
  // a side in pixels).
  google_apis::InstalledApp::IconList app_icons;
  // Drive document icon URLs for this app, paired with their size (length of
  // a side in pixels).
  google_apis::InstalledApp::IconList document_icons;
  // Web store id/extension id;
  std::string web_store_id;
  // WebApp name.
  string16 app_name;
  // Object (file) type description handled by this app.
  string16 object_type;
  // Is app the primary selector for file (default open action).
  bool is_primary_selector;
};

// Keeps the track of installed drive web application and provider in-memory.
class DriveWebAppsRegistry {
 public:
  DriveWebAppsRegistry();
  virtual ~DriveWebAppsRegistry();

  // DriveWebAppsRegistry overrides.
  virtual void GetWebAppsForFile(const base::FilePath& file,
                                 const std::string& mime_type,
                                 ScopedVector<DriveWebAppInfo>* apps);
  virtual std::set<std::string> GetExtensionsForWebStoreApp(
      const std::string& web_store_id);
  virtual void UpdateFromFeed(
      const google_apis::AccountMetadata& metadata);
  virtual void UpdateFromAppList(const google_apis::AppList& applist);

 private:
  // Defines WebApp application details that are associated with a given
  // file extension or content mimetype.
  struct WebAppFileSelector {
    WebAppFileSelector(
        const GURL& product_link,
        const google_apis::InstalledApp::IconList& app_icons,
        const google_apis::InstalledApp::IconList& document_icons,
        const string16& object_type,
        const std::string& app_id,
        bool is_primary_selector);
    ~WebAppFileSelector();
    // WebApp product link.
    GURL product_link;
    // Drive application icon URLs for this app, paired with their size (length
    // of a side in pixels).
    google_apis::InstalledApp::IconList app_icons;
    // Drive document icon URLs for this app, paired with their size (length of
    // a side in pixels).
    google_apis::InstalledApp::IconList document_icons;
    // Object (file) type description.
    string16 object_type;
    // Drive app id
    std::string app_id;
    // True if the selector is the default one. The default selector should
    // trigger on file double-click events. Non-default selectors only show up
    // in "Open with..." pop-up menu.
    bool is_primary_selector;
  };

  // Defines mapping between file content type selectors (extensions, MIME
  // types) and corresponding WebApp.
  typedef std::multimap<std::string, WebAppFileSelector*> WebAppFileSelectorMap;

  // Helper map used for deduplication of selector matching results.
  typedef std::map<const WebAppFileSelector*,
                   DriveWebAppInfo*> SelectorWebAppList;

  // Helper function for loading web application file |selectors| into
  // corresponding |map|.
  static void AddAppSelectorList(
      const GURL& product_link,
      const google_apis::InstalledApp::IconList& app_icons,
      const google_apis::InstalledApp::IconList& document_icons,
      const std::string& object_type,
      const std::string& app_id,
      bool is_primary_selector,
      const ScopedVector<std::string>& selectors,
      WebAppFileSelectorMap* map);

  // Finds matching |apps| from |map| based on provided file |selector|.
  void FindWebAppsForSelector(const std::string& selector,
                              const WebAppFileSelectorMap& map,
                              SelectorWebAppList* apps);

  // Map of web store product URL to application name.
  std::map<GURL, std::string> url_to_name_map_;

  // Map of filename extension to application info.
  WebAppFileSelectorMap webapp_extension_map_;

  // Map of MIME type to application info.
  WebAppFileSelectorMap webapp_mimetypes_map_;

  DISALLOW_COPY_AND_ASSIGN(DriveWebAppsRegistry);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_WEBAPPS_REGISTRY_H_
