// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_REPORTING_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_REPORTING_H_

#include "base/logging.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace url {
class Origin;
}

namespace content {
namespace indexed_db {

enum IndexedDBBackingStoreErrorSource {
  // 0 - 2 are no longer used.
  FIND_KEY_IN_INDEX = 3,
  GET_IDBDATABASE_METADATA,
  GET_INDEXES,
  GET_KEY_GENERATOR_CURRENT_NUMBER,
  GET_OBJECT_STORES,
  GET_RECORD,
  KEY_EXISTS_IN_OBJECT_STORE,
  LOAD_CURRENT_ROW,
  SET_UP_METADATA,
  GET_PRIMARY_KEY_VIA_INDEX,
  KEY_EXISTS_IN_INDEX,
  VERSION_EXISTS,
  DELETE_OBJECT_STORE,
  SET_MAX_OBJECT_STORE_ID,
  SET_MAX_INDEX_ID,
  GET_NEW_DATABASE_ID,
  GET_NEW_VERSION_NUMBER,
  CREATE_IDBDATABASE_METADATA,
  DELETE_DATABASE,
  TRANSACTION_COMMIT_METHOD,  // TRANSACTION_COMMIT is a WinNT.h macro
  GET_DATABASE_NAMES,
  DELETE_INDEX,
  CLEAR_OBJECT_STORE,
  READ_BLOB_JOURNAL,
  DECODE_BLOB_JOURNAL,
  GET_BLOB_KEY_GENERATOR_CURRENT_NUMBER,
  GET_BLOB_INFO_FOR_RECORD,
  INTERNAL_ERROR_MAX,
};

// Values match entries in tools/metrics/histograms/histograms.xml
enum IndexedDBBackingStoreOpenResult {
  INDEXED_DB_BACKING_STORE_OPEN_MEMORY_SUCCESS,
  INDEXED_DB_BACKING_STORE_OPEN_SUCCESS,
  INDEXED_DB_BACKING_STORE_OPEN_FAILED_DIRECTORY,
  INDEXED_DB_BACKING_STORE_OPEN_FAILED_UNKNOWN_SCHEMA,
  INDEXED_DB_BACKING_STORE_OPEN_CLEANUP_DESTROY_FAILED,
  INDEXED_DB_BACKING_STORE_OPEN_CLEANUP_REOPEN_FAILED,
  INDEXED_DB_BACKING_STORE_OPEN_CLEANUP_REOPEN_SUCCESS,
  INDEXED_DB_BACKING_STORE_OPEN_FAILED_IO_ERROR_CHECKING_SCHEMA,
  INDEXED_DB_BACKING_STORE_OPEN_FAILED_UNKNOWN_ERR_DEPRECATED,
  INDEXED_DB_BACKING_STORE_OPEN_MEMORY_FAILED,
  INDEXED_DB_BACKING_STORE_OPEN_ATTEMPT_NON_ASCII,
  INDEXED_DB_BACKING_STORE_OPEN_DISK_FULL_DEPRECATED,
  INDEXED_DB_BACKING_STORE_OPEN_ORIGIN_TOO_LONG,
  INDEXED_DB_BACKING_STORE_OPEN_NO_RECOVERY,
  INDEXED_DB_BACKING_STORE_OPEN_FAILED_PRIOR_CORRUPTION,
  INDEXED_DB_BACKING_STORE_OPEN_FAILED_CLEANUP_JOURNAL_ERROR,
  INDEXED_DB_BACKING_STORE_OPEN_MAX,
};

void HistogramOpenStatus(IndexedDBBackingStoreOpenResult result,
                         const url::Origin& origin);

void ReportInternalError(const char* type,
                         IndexedDBBackingStoreErrorSource location);

// Use to signal conditions caused by data corruption.
// A macro is used instead of an inline function so that the assert and log
// report the line number.
#define REPORT_ERROR(type, location)                      \
  do {                                                    \
    LOG(ERROR) << "IndexedDB " type " Error: " #location; \
    ::content::indexed_db::ReportInternalError(           \
        type, ::content::indexed_db::location);           \
  } while (0)

#define INTERNAL_READ_ERROR(location) REPORT_ERROR("Read", location)
#define INTERNAL_CONSISTENCY_ERROR(location) \
  REPORT_ERROR("Consistency", location)
#define INTERNAL_WRITE_ERROR(location) REPORT_ERROR("Write", location)

// Use to signal conditions that usually indicate developer error, but
// could be caused by data corruption.  A macro is used instead of an
// inline function so that the assert and log report the line number.
// TODO(cmumford): Improve test coverage so that all error conditions are
// "tested" and then delete this macro.
#define REPORT_ERROR_UNTESTED(type, location)             \
  do {                                                    \
    LOG(ERROR) << "IndexedDB " type " Error: " #location; \
    NOTREACHED();                                         \
    ::content::indexed_db::ReportInternalError(           \
        type, ::content::indexed_db::location);           \
  } while (0)

#define INTERNAL_READ_ERROR_UNTESTED(location) \
  REPORT_ERROR_UNTESTED("Read", location)
#define INTERNAL_CONSISTENCY_ERROR_UNTESTED(location) \
  REPORT_ERROR_UNTESTED("Consistency", location)
#define INTERNAL_WRITE_ERROR_UNTESTED(location) \
  REPORT_ERROR_UNTESTED("Write", location)

}  // namespace indexed_db
}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_REPORTING_H_
