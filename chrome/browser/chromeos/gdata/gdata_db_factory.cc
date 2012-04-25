// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_db_factory.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/gdata/gdata_leveldb.h"

namespace gdata {
namespace db_factory {

scoped_ptr<GDataDB> CreateGDataDB(const FilePath& db_path) {
  DVLOG(1) << "CreateGDataDB " << db_path.value();
  GDataLevelDB* level_db = new GDataLevelDB();
  level_db->Init(db_path);
  return scoped_ptr<GDataDB>(level_db);
}

}  // namespace db_factory
}  // namespace gdata
