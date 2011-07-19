// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_apps_table.h"

#include "app/sql/statement.h"
#include "base/logging.h"
#include "chrome/browser/history/history_database.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

bool WebAppsTable::Init() {
  return (InitWebAppIconsTable() && InitWebAppsTable());
}

bool WebAppsTable::IsSyncable() {
  return true;
}

bool WebAppsTable::InitWebAppIconsTable() {
  if (!db_->DoesTableExist("web_app_icons")) {
    if (!db_->Execute("CREATE TABLE web_app_icons ("
                      "url LONGVARCHAR,"
                      "width int,"
                      "height int,"
                      "image BLOB, UNIQUE (url, width, height))")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool WebAppsTable::InitWebAppsTable() {
  if (!db_->DoesTableExist("web_apps")) {
    if (!db_->Execute("CREATE TABLE web_apps ("
                      "url LONGVARCHAR UNIQUE,"
                      "has_all_images INTEGER NOT NULL)")) {
      NOTREACHED();
      return false;
    }
    if (!db_->Execute("CREATE INDEX web_apps_url_index ON web_apps (url)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool WebAppsTable::SetWebAppImage(const GURL& url, const SkBitmap& image) {
  // Don't bother with a cached statement since this will be a relatively
  // infrequent operation.
  sql::Statement s(db_->GetUniqueStatement(
      "INSERT OR REPLACE INTO web_app_icons "
      "(url, width, height, image) VALUES (?, ?, ?, ?)"));
  if (!s)
    return false;

  std::vector<unsigned char> image_data;
  gfx::PNGCodec::EncodeBGRASkBitmap(image, false, &image_data);

  s.BindString(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  s.BindInt(1, image.width());
  s.BindInt(2, image.height());
  s.BindBlob(3, &image_data.front(), static_cast<int>(image_data.size()));
  return s.Run();
}

bool WebAppsTable::GetWebAppImages(const GURL& url,
                                  std::vector<SkBitmap>* images) {
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT image FROM web_app_icons WHERE url=?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindString(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  while (s.Step()) {
    SkBitmap image;
    int col_bytes = s.ColumnByteLength(0);
    if (col_bytes > 0) {
      if (gfx::PNGCodec::Decode(
              reinterpret_cast<const unsigned char*>(s.ColumnBlob(0)),
              col_bytes, &image)) {
        images->push_back(image);
      } else {
        // Should only have valid image data in the db.
        NOTREACHED();
      }
    }
  }
  return true;
}

bool WebAppsTable::SetWebAppHasAllImages(const GURL& url,
                                        bool has_all_images) {
  sql::Statement s(db_->GetUniqueStatement(
      "INSERT OR REPLACE INTO web_apps (url, has_all_images) VALUES (?, ?)"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindString(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  s.BindInt(1, has_all_images ? 1 : 0);
  return s.Run();
}

bool WebAppsTable::GetWebAppHasAllImages(const GURL& url) {
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT has_all_images FROM web_apps WHERE url=?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindString(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  return (s.Step() && s.ColumnInt(0) == 1);
}

bool WebAppsTable::RemoveWebApp(const GURL& url) {
  sql::Statement delete_s(db_->GetUniqueStatement(
      "DELETE FROM web_app_icons WHERE url = ?"));
  if (!delete_s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  delete_s.BindString(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  if (!delete_s.Run())
    return false;

  sql::Statement delete_s2(db_->GetUniqueStatement(
      "DELETE FROM web_apps WHERE url = ?"));
  if (!delete_s2) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  delete_s2.BindString(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  return delete_s2.Run();
}

