// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/sync_db_util.h"

#include "base/files/file_path.h"
#include "base/single_thread_task_runner.h"
#include "components/sync/syncable/directory.h"
#include "sql/connection.h"

namespace syncer {

void CheckSyncDbLastModifiedTime(
    const base::FilePath& sync_dir,
    scoped_refptr<base::SingleThreadTaskRunner> callback_runner,
    base::Callback<void(base::Time)> callback) {
  const base::FilePath sync_db =
      sync_dir.Append(syncable::Directory::kSyncDatabaseFilename);

  base::File f(sync_db, base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::File::Info info;
  if (!f.IsValid() || !f.GetInfo(&info)) {
    callback_runner->PostTask(FROM_HERE, base::Bind(callback, base::Time()));
    return;
  }
  f.Close();

  sql::Connection db;
  if (!db.Open(sync_db) || !db.QuickIntegrityCheck()) {
    callback_runner->PostTask(FROM_HERE, base::Bind(callback, base::Time()));
  } else {
    callback_runner->PostTask(FROM_HERE,
                              base::Bind(callback, info.last_modified));
  }
}

}  // namespace syncer
