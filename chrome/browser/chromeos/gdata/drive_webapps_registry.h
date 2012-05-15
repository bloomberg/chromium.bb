// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_WEBAPPS_REGISTRY_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_WEBAPPS_REGISTRY_H_
#pragma once

#include <map>
#include <string>

#include "base/file_path.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/chromeos/gdata/gdata_parser.h"
#include "googleurl/src/gurl.h"

namespace gdata {

// Data structure that defines WebApp
struct DriveWebAppInfo {
  DriveWebAppInfo(const std::string& app_id,
                  const string16& app_name,
                  const string16& object_type,
                  bool is_primary_selector);
  ~DriveWebAppInfo();

  // WebApp id;
  std::string app_id;
  // WebApp name.
  string16 app_name;
  // Object (file) type description handled by this app.
  string16 object_type;
  // Is app the primary selector for file (default open action).
  bool is_primary_selector;
};

// DriveWebAppsRegistry keeps the track of installed drive web application and
// provider
class DriveWebAppsRegistry  {
 public:
  DriveWebAppsRegistry();
  virtual ~DriveWebAppsRegistry();

  // Gets the list of web |apps| matching |file| and it |mime_type|.
  void GetWebAppsForFile(const FilePath& file,
                         const std::string& mime_type,
                         ScopedVector<DriveWebAppInfo>* apps);

  // Updates the list of drive-enabled WebApps with freshly fetched account
  // metadata feed.
  void UpdateFromFeed(AccountMetadataFeed* metadata);

 private:
  // Defines WebApp application details that are associated with a given
  // file extension or content mimetype.
  struct WebAppFileSelector {
    WebAppFileSelector(const GURL& product_link,
                       const string16& object_type,
                       bool is_primary_selector);
    ~WebAppFileSelector();
    // WebApp product link.
    GURL product_link;
    // Object (file) type description.
    string16 object_type;
    // True if the selector is the default one. The default selector should
    // trigger on file double-click events. Non-default selectors only show up
    // in "Open with..." pop-up menu.
    bool is_primary_selector;
  };

  // Defines mapping between file content type selectors (extensions, mime
  // types) and corresponding WebApp.
  typedef std::multimap<std::string, WebAppFileSelector*> WebAppFileSelectorMap;

  // Helper map used for deduplication of selector matching results.
  typedef std::map<const WebAppFileSelector*,
                   DriveWebAppInfo*> SelectorWebAppList;

  // Helper function for loading webapp file |selectors| into corresponding
  // |map|.
  static void AddAppSelectorList(const GURL& product_link,
                                 const string16& object_type,
                                 bool is_primary_selector,
                                 const ScopedVector<std::string>& selectors,
                                 WebAppFileSelectorMap* map);

  // Finds matching |apps| from |map| based on provided file |selector|.
  void FindWebAppsForSelector(const std::string& selector,
                              const WebAppFileSelectorMap& map,
                              SelectorWebAppList* apps);


  std::map<GURL, string16> webapp_map_;
  WebAppFileSelectorMap webapp_extension_map_;
  WebAppFileSelectorMap webapp_mimetypes_map_;

  DISALLOW_COPY_AND_ASSIGN(DriveWebAppsRegistry);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_WEBAPPS_REGISTRY_H_
