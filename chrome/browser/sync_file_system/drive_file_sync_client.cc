// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_file_sync_client.h"

#include <sstream>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/google_apis/drive_uploader.h"
#include "chrome/browser/google_apis/gdata_wapi_service.h"
#include "net/base/escape.h"

namespace sync_file_system {

namespace {

const char kRootResourceId[] = "";
const char kSyncRootDirectoryName[] = "Chrome Syncable FileSystem";
const char kResourceLinkPrefix[] =
    "https://docs.google.com/feeds/default/private/full/";

bool HasParentLinkTo(const ScopedVector<google_apis::Link>& links,
                     const std::string& parent_resource_id) {
  bool should_not_have_parent = parent_resource_id.empty();
  GURL parent_link(kResourceLinkPrefix + net::EscapePath(parent_resource_id));

  for (ScopedVector<google_apis::Link>::const_iterator itr = links.begin();
       itr != links.end(); ++itr) {
    if ((*itr)->type() == google_apis::Link::LINK_PARENT) {
      if (should_not_have_parent)
        return false;
      if ((*itr)->href() == parent_link)
        return true;
    }
  }

  return should_not_have_parent;
}

google_apis::DocumentEntry* GetDocumentByTitleAndParent(
    const ScopedVector<google_apis::DocumentEntry>& entries,
    const std::string& parent_resource_id,
    const string16& title) {
  typedef ScopedVector<google_apis::DocumentEntry>::const_iterator iterator;
  for (iterator itr = entries.begin(); itr != entries.end(); ++itr) {
    if ((*itr)->title() == title &&
        HasParentLinkTo((*itr)->links(), parent_resource_id)) {
      return *itr;
    }
  }
  return NULL;
}

}  // namespace

DriveFileSyncClient::DriveFileSyncClient(Profile* profile) {
  drive_service_.reset(new google_apis::GDataWapiService);
  drive_service_->Initialize(profile);

  drive_uploader_.reset(new google_apis::DriveUploader(drive_service_.get()));
}

scoped_ptr<DriveFileSyncClient> DriveFileSyncClient::CreateForTesting(
    Profile* profile,
    scoped_ptr<google_apis::DriveServiceInterface> drive_service,
    scoped_ptr<google_apis::DriveUploaderInterface> drive_uploader) {
  return make_scoped_ptr(new DriveFileSyncClient(
      profile, drive_service.Pass(), drive_uploader.Pass()));
}

DriveFileSyncClient::DriveFileSyncClient(
    Profile* profile,
    scoped_ptr<google_apis::DriveServiceInterface> drive_service,
    scoped_ptr<google_apis::DriveUploaderInterface> drive_uploader) {
  drive_service_ = drive_service.Pass();
  drive_service_->Initialize(profile);

  drive_uploader_ = drive_uploader.Pass();
}

DriveFileSyncClient::~DriveFileSyncClient() {
  DCHECK(CalledOnValidThread());
}

void DriveFileSyncClient::GetDriveDirectoryForSyncRoot(
    const ResourceIdCallback& callback) {
  DCHECK(CalledOnValidThread());

  std::string directory_name(kSyncRootDirectoryName);
  SearchFilesInDirectory(
      kRootResourceId,
      FormatTitleQuery(directory_name),
      base::Bind(&DriveFileSyncClient::DidGetDirectory, AsWeakPtr(),
                 kRootResourceId, directory_name, callback));
}

void DriveFileSyncClient::GetDriveDirectoryForOrigin(
    const std::string& sync_root_resource_id,
    const GURL& origin,
    const ResourceIdCallback& callback) {
  DCHECK(CalledOnValidThread());

  std::string directory_name(origin.spec());
  SearchFilesInDirectory(
      sync_root_resource_id,
      FormatTitleQuery(directory_name),
      base::Bind(&DriveFileSyncClient::DidGetDirectory, AsWeakPtr(),
                 sync_root_resource_id, directory_name, callback));
}

void DriveFileSyncClient::DidGetDirectory(
    const std::string& parent_resource_id,
    const std::string& directory_name,
    const ResourceIdCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::DocumentFeed> feed) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsStringASCII(directory_name));

  if (error != google_apis::HTTP_SUCCESS) {
    callback.Run(error, std::string());
    return;
  }

  google_apis::DocumentEntry* entry =
      GetDocumentByTitleAndParent(feed->entries(),
                                  parent_resource_id,
                                  ASCIIToUTF16(directory_name));
  if (!entry) {
    if (parent_resource_id.empty()) {
      // Use empty content URL for root directory.
      drive_service_->AddNewDirectory(
          GURL(),  // parent_content_url
          FilePath().AppendASCII(directory_name).value(),
          base::Bind(&DriveFileSyncClient::DidCreateDirectory,
                     AsWeakPtr(), callback));
      return;
    }
    drive_service_->GetDocumentEntry(
        parent_resource_id,
        base::Bind(
            &DriveFileSyncClient::DidGetParentDirectoryForCreateDirectory,
            AsWeakPtr(), FilePath().AppendASCII(directory_name).value(),
            callback));
    return;
  }

  // TODO(tzik): Handle error.
  DCHECK_EQ(google_apis::ENTRY_KIND_FOLDER, entry->kind());
  DCHECK_EQ(ASCIIToUTF16(directory_name), entry->title());

  callback.Run(error, entry->resource_id());
}

void DriveFileSyncClient::DidGetParentDirectoryForCreateDirectory(
    const FilePath::StringType& directory_name,
    const ResourceIdCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<base::Value> data) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    callback.Run(error, std::string());
    return;
  }

  scoped_ptr<google_apis::DocumentEntry> entry(
      google_apis::DocumentEntry::ExtractAndParse(*data));
  drive_service_->AddNewDirectory(
      entry->content_url(),
      directory_name,
      base::Bind(&DriveFileSyncClient::DidCreateDirectory,
                 AsWeakPtr(), callback));
}

void DriveFileSyncClient::DidCreateDirectory(
    const ResourceIdCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<base::Value> data) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS &&
      error != google_apis::HTTP_CREATED) {
    callback.Run(error, std::string());
    return;
  }
  error = google_apis::HTTP_CREATED;

  // TODO(tzik): Confirm if there's no confliction. If another client tried
  // to create the directory, we might make duplicated directories.
  scoped_ptr<google_apis::DocumentEntry> entry(
      google_apis::DocumentEntry::ExtractAndParse(*data));
  callback.Run(error, entry->resource_id());
}

void DriveFileSyncClient::GetLargestChangeStamp(
    const ChangeStampCallback& callback) {
  DCHECK(CalledOnValidThread());

  drive_service_->GetAccountMetadata(
      base::Bind(&DriveFileSyncClient::DidGetAccountMetadata,
                 AsWeakPtr(), callback));
}

void DriveFileSyncClient::DidGetAccountMetadata(
    const ChangeStampCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<base::Value> data) {
  DCHECK(CalledOnValidThread());

  int64 largest_changestamp = 0;
  if (error == google_apis::HTTP_SUCCESS) {
    scoped_ptr<google_apis::AccountMetadataFeed> metadata(
        google_apis::AccountMetadataFeed::CreateFrom(*data));
    largest_changestamp = metadata->largest_changestamp();
  }

  callback.Run(error, largest_changestamp);
}

void DriveFileSyncClient::SearchFilesInDirectory(
    const std::string& directory_resource_id,
    const std::string& search_query,
    const DocumentFeedCallback& callback) {
  DCHECK(CalledOnValidThread());

  drive_service_->GetDocuments(
      GURL(),  // feed_url
      0,  // start_changestamp
      search_query,
      false,  // shared_with_me
      directory_resource_id,
      base::Bind(&DriveFileSyncClient::DidGetDocumentFeedData,
                 AsWeakPtr(), callback));
}

void DriveFileSyncClient::ListFiles(const std::string& directory_resource_id,
                                    const DocumentFeedCallback& callback) {
  DCHECK(CalledOnValidThread());

  SearchFilesInDirectory(directory_resource_id,
                         std::string() /* search_query */,
                         callback);
}

void DriveFileSyncClient::ListChanges(int64 start_changestamp,
                                      const DocumentFeedCallback& callback) {
  DCHECK(CalledOnValidThread());
  drive_service_->GetDocuments(
      GURL(),  // feed_url
      start_changestamp,
      std::string(),  // search_query
      false,  // shared_with_me
      std::string(),  // directory_resource_id
      base::Bind(&DriveFileSyncClient::DidGetDocumentFeedData,
                 AsWeakPtr(), callback));
}

void DriveFileSyncClient::ContinueListing(
    const GURL& feed_url,
    const DocumentFeedCallback& callback) {
  DCHECK(CalledOnValidThread());
  drive_service_->GetDocuments(
      feed_url,
      0,  // start_changestamp
      std::string(),  // search_query
      false,  // shared_with_me
      std::string(),  // directory_resource_id
      base::Bind(&DriveFileSyncClient::DidGetDocumentFeedData,
                 AsWeakPtr(), callback));
}

void DriveFileSyncClient::DownloadFile(
    const std::string& resource_id,
    const std::string& local_file_md5,
    const FilePath& local_file_path,
    const DownloadFileCallback& callback) {
  // TODO(nhiroki): implement.
  NOTIMPLEMENTED();
}

void DriveFileSyncClient::UploadNewFile(
    const std::string& directory_resource_id,
    const FilePath& local_file_path,
    const std::string& title,
    int64 file_size,
    const UploadFileCallback& callback) {
  // TODO(nhiroki): implement.
  NOTIMPLEMENTED();
}

void DriveFileSyncClient::UploadExistingFile(
    const std::string& resource_id,
    const std::string& remote_file_md5,
    const FilePath& local_file_path,
    int64 file_size,
    const UploadFileCallback& callback) {
  // TODO(nhiroki): implement.
  NOTIMPLEMENTED();
}

void DriveFileSyncClient::DeleteFile(
    const std::string& resource_id,
    const std::string& remote_file_md5,
    const GDataErrorCallback& callback) {
  // TODO(nhiroki): implement.
  NOTIMPLEMENTED();
}

void DriveFileSyncClient::DidGetDocumentFeedData(
    const DocumentFeedCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<base::Value> data) {
  DCHECK(CalledOnValidThread());

  callback.Run(error,
               error == google_apis::HTTP_SUCCESS ?
                   google_apis::DocumentFeed::ExtractAndParse(*data) :
                   scoped_ptr<google_apis::DocumentFeed>());
}

// static
std::string DriveFileSyncClient::FormatTitleQuery(const std::string& title) {
  // TODO(tzik): This pattern matches partial and case-insensitive,
  // and also matches files in subdirectories.
  // Refine the query after we migrate to Drive API.
  std::ostringstream out;
  out << "title:";

  // Escape single quote and back slash with '\\'.
  // https://developers.google.com/drive/search-parameters
  out << '\'';
  for (std::string::const_iterator itr = title.begin();
       itr != title.end(); ++itr) {
    switch (*itr) {
      case '\'':
      case '\\':
        out << '\\' << *itr;
        break;
      default:
        out << *itr;
        break;
    }
  }
  out << '\'';
  return out.str();
}

}  // namespace sync_file_system
