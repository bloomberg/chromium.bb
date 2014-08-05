// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"

#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/leveldb_wrapper.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/gdata_wapi_parser.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace sync_file_system {
namespace drive_backend {

void PutVersionToDB(int64 version, LevelDBWrapper* db) {
  if (db)
    db->Put(kDatabaseVersionKey, base::Int64ToString(version));
}

void PutServiceMetadataToDB(const ServiceMetadata& service_metadata,
                            LevelDBWrapper* db) {
  if (!db)
    return;

  std::string value;
  bool success = service_metadata.SerializeToString(&value);
  DCHECK(success);
  db->Put(kServiceMetadataKey, value);
}

void PutFileMetadataToDB(const FileMetadata& file, LevelDBWrapper* db) {
  if (!db)
    return;

  std::string value;
  bool success = file.SerializeToString(&value);
  DCHECK(success);
  db->Put(kFileMetadataKeyPrefix + file.file_id(), value);
}

void PutFileTrackerToDB(const FileTracker& tracker, LevelDBWrapper* db) {
  if (!db)
    return;

  std::string value;
  bool success = tracker.SerializeToString(&value);
  DCHECK(success);
  db->Put(kFileTrackerKeyPrefix + base::Int64ToString(tracker.tracker_id()),
          value);
}

void PutFileMetadataDeletionToDB(const std::string& file_id,
                                 LevelDBWrapper* db) {
  if (db)
    db->Delete(kFileMetadataKeyPrefix + file_id);
}

void PutFileTrackerDeletionToDB(int64 tracker_id, LevelDBWrapper* db) {
  if (db)
    db->Delete(kFileTrackerKeyPrefix + base::Int64ToString(tracker_id));
}

bool HasFileAsParent(const FileDetails& details, const std::string& file_id) {
  for (int i = 0; i < details.parent_folder_ids_size(); ++i) {
    if (details.parent_folder_ids(i) == file_id)
      return true;
  }
  return false;
}

bool IsAppRoot(const FileTracker& tracker) {
  return tracker.tracker_kind() == TRACKER_KIND_APP_ROOT ||
      tracker.tracker_kind() == TRACKER_KIND_DISABLED_APP_ROOT;
}

std::string GetTrackerTitle(const FileTracker& tracker) {
  if (tracker.has_synced_details())
    return tracker.synced_details().title();
  return std::string();
}

SyncStatusCode GDataErrorCodeToSyncStatusCode(
    google_apis::GDataErrorCode error) {
  // NOTE: Please update DriveFileSyncService::UpdateServiceState when you add
  // more error code mapping.
  switch (error) {
    case google_apis::HTTP_SUCCESS:
    case google_apis::HTTP_CREATED:
    case google_apis::HTTP_NO_CONTENT:
    case google_apis::HTTP_FOUND:
      return SYNC_STATUS_OK;

    case google_apis::HTTP_NOT_MODIFIED:
      return SYNC_STATUS_NOT_MODIFIED;

    case google_apis::HTTP_CONFLICT:
    case google_apis::HTTP_PRECONDITION:
      return SYNC_STATUS_HAS_CONFLICT;

    case google_apis::HTTP_UNAUTHORIZED:
      return SYNC_STATUS_AUTHENTICATION_FAILED;

    case google_apis::GDATA_NO_CONNECTION:
      return SYNC_STATUS_NETWORK_ERROR;

    case google_apis::HTTP_INTERNAL_SERVER_ERROR:
    case google_apis::HTTP_BAD_GATEWAY:
    case google_apis::HTTP_SERVICE_UNAVAILABLE:
    case google_apis::GDATA_CANCELLED:
    case google_apis::GDATA_NOT_READY:
      return SYNC_STATUS_SERVICE_TEMPORARILY_UNAVAILABLE;

    case google_apis::HTTP_NOT_FOUND:
    case google_apis::HTTP_GONE:
      return SYNC_FILE_ERROR_NOT_FOUND;

    case google_apis::GDATA_FILE_ERROR:
      return SYNC_FILE_ERROR_FAILED;

    case google_apis::HTTP_FORBIDDEN:
      return SYNC_STATUS_ACCESS_FORBIDDEN;

    case google_apis::HTTP_RESUME_INCOMPLETE:
    case google_apis::HTTP_BAD_REQUEST:
    case google_apis::HTTP_LENGTH_REQUIRED:
    case google_apis::HTTP_NOT_IMPLEMENTED:
    case google_apis::GDATA_PARSE_ERROR:
    case google_apis::GDATA_OTHER_ERROR:
      return SYNC_STATUS_FAILED;

    case google_apis::GDATA_NO_SPACE:
      return SYNC_FILE_ERROR_NO_SPACE;
  }

  // There's a case where DriveService layer returns GDataErrorCode==-1
  // when network is unavailable. (http://crbug.com/223042)
  // TODO(kinuko,nhiroki): We should identify from where this undefined error
  // code is coming.
  if (error == -1)
    return SYNC_STATUS_NETWORK_ERROR;

  util::Log(logging::LOG_WARNING,
            FROM_HERE,
            "Got unexpected error: %d",
            static_cast<int>(error));
  return SYNC_STATUS_FAILED;
}

bool RemovePrefix(const std::string& str, const std::string& prefix,
                  std::string* out) {
  if (!StartsWithASCII(str, prefix, true)) {
    if (out)
      *out = str;
    return false;
  }

  if (out)
    *out = str.substr(prefix.size());
  return true;
}

scoped_ptr<ServiceMetadata> InitializeServiceMetadata(LevelDBWrapper* db) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(db);

  std::string value;
  leveldb::Status status = db->Get(kServiceMetadataKey, &value);

  scoped_ptr<ServiceMetadata> service_metadata(new ServiceMetadata);
  if (!status.ok() || !service_metadata->ParseFromString(value))
    service_metadata->set_next_tracker_id(1);

  return service_metadata.Pass();
}

}  // namespace drive_backend
}  // namespace sync_file_system
