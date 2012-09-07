// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_WEBAPPS_REGISTRY_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_WEBAPPS_REGISTRY_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/scoped_vector.h"
#include "chrome/browser/chromeos/gdata/gdata_wapi_parser.h"
#include "googleurl/src/gurl.h"

class FilePath;

namespace gdata {

class AppList;

// Data structure that defines WebApp
struct DriveWebAppInfo {
  DriveWebAppInfo(const std::string& app_id,
                  const InstalledApp::IconList& app_icons,
                  const InstalledApp::IconList& document_icons,
                  const std::string& web_store_id,
                  const string16& app_name,
                  const string16& object_type,
                  bool is_primary_selector);
  ~DriveWebAppInfo();

  // Drive app id
  std::string app_id;
  // Drive application icon URLs for this app, paired with their size (length of
  // a side in pixels).
  InstalledApp::IconList app_icons;
  // Drive document icon URLs for this app, paired with their size (length of
  // a side in pixels).
  InstalledApp::IconList document_icons;
  // Web store id/extension id;
  std::string web_store_id;
  // WebApp name.
  string16 app_name;
  // Object (file) type description handled by this app.
  string16 object_type;
  // Is app the primary selector for file (default open action).
  bool is_primary_selector;
};

// DriveWebAppsRegistry abstraction layer.
// The interface is defined to make DriveWebAppsRegistry mockable.
class DriveWebAppsRegistryInterface {
 public:
  virtual ~DriveWebAppsRegistryInterface() {}

  // Gets the list of web |apps| matching |file| and its |mime_type|.
  virtual void GetWebAppsForFile(const FilePath& file,
                                 const std::string& mime_type,
                                 ScopedVector<DriveWebAppInfo>* apps) = 0;

  // Returns a set of filename extensions registered for the given
  // |web_store_id|.
  virtual std::set<std::string> GetExtensionsForWebStoreApp(
      const std::string& web_store_id) = 0;

  // Updates the list of drive-enabled WebApps with freshly fetched account
  // metadata feed.
  virtual void UpdateFromFeed(const AccountMetadataFeed& metadata) = 0;

  // Updates the list of drive-enabled WebApps with freshly fetched account
  // metadata feed.
  virtual void UpdateFromApplicationList(const AppList& applist) = 0;
};

// The production implementation of DriveWebAppsRegistryInterface.
// Keeps the track of installed drive web application and provider.
class DriveWebAppsRegistry : public DriveWebAppsRegistryInterface {
 public:
  DriveWebAppsRegistry();
  virtual ~DriveWebAppsRegistry();

  // DriveWebAppsRegistry overrides.
  virtual void GetWebAppsForFile(const FilePath& file,
                                 const std::string& mime_type,
                                 ScopedVector<DriveWebAppInfo>* apps) OVERRIDE;
  virtual std::set<std::string> GetExtensionsForWebStoreApp(
      const std::string& web_store_id) OVERRIDE;
  virtual void UpdateFromFeed(const AccountMetadataFeed& metadata) OVERRIDE;
  virtual void UpdateFromApplicationList(const AppList& applist) OVERRIDE;

 private:
  // Defines WebApp application details that are associated with a given
  // file extension or content mimetype.
  struct WebAppFileSelector {
    WebAppFileSelector(const GURL& product_link,
                       const InstalledApp::IconList& app_icons,
                       const InstalledApp::IconList& document_icons,
                       const string16& object_type,
                       const std::string& app_id,
                       bool is_primary_selector);
    ~WebAppFileSelector();
    // WebApp product link.
    GURL product_link;
    // Drive application icon URLs for this app, paired with their size (length
    // of a side in pixels).
    InstalledApp::IconList app_icons;
    // Drive document icon URLs for this app, paired with their size (length of
    // a side in pixels).
    InstalledApp::IconList document_icons;
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
  static void AddAppSelectorList(const GURL& product_link,
                                 const InstalledApp::IconList& app_icons,
                                 const InstalledApp::IconList& document_icons,
                                 const string16& object_type,
                                 const std::string& app_id,
                                 bool is_primary_selector,
                                 const ScopedVector<std::string>& selectors,
                                 WebAppFileSelectorMap* map);

  // Finds matching |apps| from |map| based on provided file |selector|.
  void FindWebAppsForSelector(const std::string& selector,
                              const WebAppFileSelectorMap& map,
                              SelectorWebAppList* apps);

  // Map of web store product URL to application name.
  std::map<GURL, string16> url_to_name_map_;

  // Map of filename extension to application info.
  WebAppFileSelectorMap webapp_extension_map_;

  // Map of MIME type to application info.
  WebAppFileSelectorMap webapp_mimetypes_map_;

  DISALLOW_COPY_AND_ASSIGN(DriveWebAppsRegistry);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_WEBAPPS_REGISTRY_H_
