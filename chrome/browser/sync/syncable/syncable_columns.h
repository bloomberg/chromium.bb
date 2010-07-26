// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_SYNCABLE_COLUMNS_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_SYNCABLE_COLUMNS_H_
#pragma once

#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/syncable/syncable_changes_version.h"

namespace syncable {

struct ColumnSpec {
  const char* name;
  const char* spec;
};

// Must be in exact same order as fields in syncable.
static const ColumnSpec g_metas_columns[] = {
  //////////////////////////////////////
  // int64s
  {"metahandle", "bigint primary key ON CONFLICT FAIL"},
  {"base_version", "bigint default " CHANGES_VERSION_STRING},
  {"server_version", "bigint default 0"},
  // These timestamps are kept in native file timestamp format.  It is
  // up to the syncer to translate to Java time when syncing.
  {"mtime", "bigint default 0"},
  {"server_mtime", "bigint default 0"},
  {"ctime", "bigint default 0"},
  {"server_ctime", "bigint default 0"},
  {"server_position_in_parent", "bigint default 0"},
  // This is the item ID that we store for the embedding application.
  {"local_external_id", "bigint default 0"},
  //////////////////////////////////////
  // Ids
  {"id", "varchar(255) default \"r\""},
  {"parent_id", "varchar(255) default \"r\""},
  {"server_parent_id", "varchar(255) default \"r\""},
  {"prev_id", "varchar(255) default \"r\""},
  {"next_id", "varchar(255) default \"r\""},
  //////////////////////////////////////
  // bits
  {"is_unsynced", "bit default 0"},
  {"is_unapplied_update", "bit default 0"},
  {"is_del", "bit default 0"},
  {"is_dir", "bit default 0"},
  {"server_is_dir", "bit default 0"},
  {"server_is_del", "bit default 0"},
  //////////////////////////////////////
  // Strings
  {"non_unique_name", "varchar"},
  {"server_non_unique_name", "varchar(255)"},
  {"unique_server_tag", "varchar"},
  {"unique_client_tag", "varchar"},
  //////////////////////////////////////
  // Blobs.
  {"specifics", "blob"},
  {"server_specifics", "blob"},
};

// At least enforce that there are equal number of column names and fields.
COMPILE_ASSERT(arraysize(g_metas_columns) >= FIELD_COUNT, missing_column_name);
COMPILE_ASSERT(arraysize(g_metas_columns) <= FIELD_COUNT, extra_column_names);

static inline const char* ColumnName(int field) {
  DCHECK(field < BEGIN_TEMPS);
  return g_metas_columns[field].name;
}

}  // namespace syncable

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_SYNCABLE_COLUMNS_H_
