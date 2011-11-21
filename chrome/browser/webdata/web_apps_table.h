// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_WEB_APPS_TABLE_H_
#define CHROME_BROWSER_WEBDATA_WEB_APPS_TABLE_H_
#pragma once

#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/webdata/web_database_table.h"

class GURL;
class SkBitmap;

// This class manages the WebApps tables within the SQLite database passed to
// the constructor. It expects the following schema:
//
// Note: The database stores time in seconds, UTC.
//
// web_apps
//   url                 URL of the web app.
//   has_all_images      Do we have all the images?
//
// web_app_icons
//   url         URL of the web app.
//   width       Width of the image.
//   height      Height of the image.
//   image       PNG encoded image data.
//
class WebAppsTable : public WebDatabaseTable {
 public:
  WebAppsTable(sql::Connection* db, sql::MetaTable* meta_table)
      : WebDatabaseTable(db, meta_table) {}
  virtual ~WebAppsTable() {}
  virtual bool Init() OVERRIDE;
  virtual bool IsSyncable() OVERRIDE;

  bool SetWebAppImage(const GURL& url, const SkBitmap& image);
  bool GetWebAppImages(const GURL& url, std::vector<SkBitmap>* images);

  bool SetWebAppHasAllImages(const GURL& url, bool has_all_images);
  bool GetWebAppHasAllImages(const GURL& url);

  bool RemoveWebApp(const GURL& url);

 private:
  bool InitWebAppIconsTable();
  bool InitWebAppsTable();

  DISALLOW_COPY_AND_ASSIGN(WebAppsTable);
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_APPS_TABLE_H_
