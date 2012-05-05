// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/android/favicon_sql_handler.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/history/thumbnail_database.h"

using base::Time;

namespace history {

namespace {

// The interesting columns of this handler.
const HistoryAndBookmarkRow::ColumnID kInterestingColumns[] = {
  HistoryAndBookmarkRow::FAVICON};

} // namespace

FaviconSQLHandler::FaviconSQLHandler(ThumbnailDatabase* thumbnail_db)
    : SQLHandler(kInterestingColumns, arraysize(kInterestingColumns)),
      thumbnail_db_(thumbnail_db) {
}

FaviconSQLHandler::~FaviconSQLHandler() {
}

bool FaviconSQLHandler::Update(const HistoryAndBookmarkRow& row,
                               const TableIDRows& ids_set) {
  FaviconID favicon_id = 0;
  if (!row.favicon().empty()) {
    // If the image_data will be updated, it is not reasonable to find if the
    // icon is already in database, just create a new favicon.
    favicon_id = thumbnail_db_->AddFavicon(GURL(), history::FAVICON);
    if (!favicon_id)
      return false;

    scoped_refptr<base::RefCountedMemory> image_data =
        new base::RefCountedBytes(row.favicon());
    if (!thumbnail_db_->SetFavicon(favicon_id, image_data, Time::Now()))
      return false;
  }

  std::vector<FaviconID> favicon_ids;
  for (TableIDRows::const_iterator i = ids_set.begin();
       i != ids_set.end(); ++i) {
    IconMapping icon_mapping;
    if (thumbnail_db_->GetIconMappingForPageURL(i->url, FAVICON,
                                                &icon_mapping)) {
      if (favicon_id) {
        if (!thumbnail_db_->UpdateIconMapping(icon_mapping.mapping_id,
                                              favicon_id))
          return false;
      } else {
        // Require to delete the icon mapping.
        if (!thumbnail_db_->DeleteIconMappings(i->url))
          return false;
      }
      // Keep the old icon for deleting it later if possible.
      favicon_ids.push_back(icon_mapping.icon_id);
    } else if (favicon_id) {
      // The URL doesn't have icon before, add the icon mapping.
      if (!thumbnail_db_->AddIconMapping(i->url, favicon_id))
        return false;
    }
  }
  // As we update the favicon, Let's remove unused favicons if any.
  if (!favicon_ids.empty() && !DeleteUnusedFavicon(favicon_ids))
    return false;

  return true;
}

bool FaviconSQLHandler::Delete(const TableIDRows& ids_set) {
  std::vector<FaviconID> favicon_ids;
  for (TableIDRows::const_iterator i = ids_set.begin();
       i != ids_set.end(); ++i) {
    // Since the URL was delete, we delete all type of icon mapping.
    IconMapping icon_mapping;
    if (thumbnail_db_->GetIconMappingForPageURL(i->url, FAVICON,
                                                &icon_mapping))
      favicon_ids.push_back(icon_mapping.icon_id);
    if (thumbnail_db_->GetIconMappingForPageURL(i->url, TOUCH_ICON,
                                                &icon_mapping))
      favicon_ids.push_back(icon_mapping.icon_id);
    if (thumbnail_db_->GetIconMappingForPageURL(i->url,
            TOUCH_PRECOMPOSED_ICON, &icon_mapping))
      favicon_ids.push_back(icon_mapping.icon_id);
    if (!thumbnail_db_->DeleteIconMappings(i->url))
      return false;
  }

  if (favicon_ids.empty())
    return true;

  if (!DeleteUnusedFavicon(favicon_ids))
    return false;

  return true;
}

bool FaviconSQLHandler::Insert(HistoryAndBookmarkRow* row) {
  if (!row->is_value_set_explicitly(HistoryAndBookmarkRow::FAVICON) ||
      row->favicon().empty())
    return true;

  DCHECK(row->is_value_set_explicitly(HistoryAndBookmarkRow::URL));

  // Is it a problem to give a empty URL?
  FaviconID id = thumbnail_db_->AddFavicon(GURL(), history::FAVICON);
  if (!id)
    return false;

  scoped_refptr<base::RefCountedMemory> image_data =
      new base::RefCountedBytes(row->favicon());
  if (!thumbnail_db_->SetFavicon(id, image_data, Time::Now()))
    return false;
  return thumbnail_db_->AddIconMapping(row->url(), id);
}

bool FaviconSQLHandler::DeleteUnusedFavicon(const std::vector<FaviconID>& ids) {
  for (std::vector<FaviconID>::const_iterator i = ids.begin(); i != ids.end();
       ++i) {
    if (!thumbnail_db_->HasMappingFor(*i) && !thumbnail_db_->DeleteFavicon(*i))
      return false;
  }
  return true;
}

}  // namespace history.
