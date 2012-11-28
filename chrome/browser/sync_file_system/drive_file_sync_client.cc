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
#include "chrome/browser/google_apis/gdata_wapi_url_generator.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"

namespace sync_file_system {

namespace {

const char kRootResourceId[] = "";
const char kSyncRootDirectoryName[] = "Chrome Syncable FileSystem";
const char kResourceLinkPrefix[] =
    "https://docs.google.com/feeds/default/private/full/";
const char kMimeTypeOctetStream[] = "application/octet-stream";

// This path is not actually used but is required by DriveUploaderInterface.
const FilePath::CharType kDummyDrivePath[] =
    FILE_PATH_LITERAL("/dummy/drive/path");

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
  drive_service_.reset(new google_apis::GDataWapiService(
      GURL(google_apis::GDataWapiUrlGenerator::kBaseUrlForProduction),
      "" /* custom_user_agent */));
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
  drive_service_->CancelAll();
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

  google_apis::DocumentEntry* entry = GetDocumentByTitleAndParent(
      feed->entries(), parent_resource_id, ASCIIToUTF16(directory_name));
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
  DCHECK(data);

  scoped_ptr<google_apis::DocumentEntry> entry(
      google_apis::DocumentEntry::ExtractAndParse(*data));
  if (!entry) {
    callback.Run(google_apis::GDATA_PARSE_ERROR, std::string());
    return;
  }

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
  DCHECK(entry);
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
  DCHECK(CalledOnValidThread());
  drive_service_->GetDocumentEntry(
      resource_id,
      base::Bind(&DriveFileSyncClient::DidGetDocumentEntryData,
                 AsWeakPtr(),
                 base::Bind(&DriveFileSyncClient::DownloadFileInternal,
                            AsWeakPtr(), local_file_md5, local_file_path,
                            callback)));
}

void DriveFileSyncClient::UploadNewFile(
    const std::string& directory_resource_id,
    const FilePath& local_file_path,
    const std::string& title,
    int64 file_size,
    const UploadFileCallback& callback) {
  DCHECK(CalledOnValidThread());
  drive_service_->GetDocumentEntry(
      directory_resource_id,
      base::Bind(&DriveFileSyncClient::DidGetDocumentEntryData,
                 AsWeakPtr(),
                 base::Bind(&DriveFileSyncClient::UploadNewFileInternal,
                            AsWeakPtr(), local_file_path, title, file_size,
                            callback)));
}

void DriveFileSyncClient::UploadExistingFile(
    const std::string& resource_id,
    const std::string& remote_file_md5,
    const FilePath& local_file_path,
    int64 file_size,
    const UploadFileCallback& callback) {
  DCHECK(CalledOnValidThread());
  drive_service_->GetDocumentEntry(
      resource_id,
      base::Bind(&DriveFileSyncClient::DidGetDocumentEntryData,
                 AsWeakPtr(),
                 base::Bind(&DriveFileSyncClient::UploadExistingFileInternal,
                            AsWeakPtr(), remote_file_md5, local_file_path,
                            file_size, callback)));
}

void DriveFileSyncClient::DeleteFile(
    const std::string& resource_id,
    const std::string& remote_file_md5,
    const GDataErrorCallback& callback) {
  DCHECK(CalledOnValidThread());
  drive_service_->GetDocumentEntry(
      resource_id,
      base::Bind(&DriveFileSyncClient::DidGetDocumentEntryData,
                 AsWeakPtr(),
                 base::Bind(&DriveFileSyncClient::DeleteFileInternal,
                            AsWeakPtr(), remote_file_md5, callback)));
}

void DriveFileSyncClient::DidGetDocumentFeedData(
    const DocumentFeedCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<base::Value> data) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    callback.Run(error, scoped_ptr<google_apis::DocumentFeed>());
    return;
  }

  DCHECK(data);
  scoped_ptr<google_apis::DocumentFeed> feed(
      google_apis::DocumentFeed::ExtractAndParse(*data));
  if (!feed)
    error = google_apis::GDATA_PARSE_ERROR;
  callback.Run(error, feed.Pass());
}

void DriveFileSyncClient::DidGetDocumentEntryData(
    const DocumentEntryCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<base::Value> data) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    callback.Run(error, scoped_ptr<google_apis::DocumentEntry>());
    return;
  }

  DCHECK(data);
  scoped_ptr<google_apis::DocumentEntry> entry(
      google_apis::DocumentEntry::ExtractAndParse(*data));
  if (!entry)
    error = google_apis::GDATA_PARSE_ERROR;
  callback.Run(error, entry.Pass());
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

void DriveFileSyncClient::DownloadFileInternal(
    const std::string& local_file_md5,
    const FilePath& local_file_path,
    const DownloadFileCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::DocumentEntry> entry) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    callback.Run(error, std::string());
    return;
  }
  DCHECK(entry);

  // If local file and remote file are same, cancel the download.
  if (local_file_md5 == entry->file_md5()) {
    callback.Run(google_apis::HTTP_NOT_MODIFIED, local_file_md5);
    return;
  }

  // TODO(nhiroki): support ETag. Currently we assume there is no change between
  // GetDocumentEntry and DownloadFile call.
  drive_service_->DownloadFile(
      FilePath(kDummyDrivePath),
      local_file_path,
      entry->content_url(),
      base::Bind(&DriveFileSyncClient::DidDownloadFile,
                 AsWeakPtr(), entry->file_md5(), callback),
      google_apis::GetContentCallback());
}

void DriveFileSyncClient::DidDownloadFile(
    const std::string& downloaded_file_md5,
    const DownloadFileCallback& callback,
    google_apis::GDataErrorCode error,
    const FilePath& downloaded_file_path) {
  DCHECK(CalledOnValidThread());
  callback.Run(error, downloaded_file_md5);
}

void DriveFileSyncClient::UploadNewFileInternal(
    const FilePath& local_file_path,
    const std::string& title,
    int64 file_size,
    const UploadFileCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::DocumentEntry> parent_directory_entry) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    callback.Run(error, std::string(), std::string());
    return;
  }
  DCHECK(parent_directory_entry);

  std::string mime_type;
  if (!net::GetWellKnownMimeTypeFromExtension(
          local_file_path.Extension(), &mime_type))
    mime_type = kMimeTypeOctetStream;

  drive_uploader_->UploadNewFile(
      parent_directory_entry->GetLinkByType(
          google_apis::Link::LINK_RESUMABLE_CREATE_MEDIA)->href(),
      FilePath(kDummyDrivePath),
      local_file_path,
      title,
      mime_type,
      file_size,  // content_length.
      file_size,
      base::Bind(&DriveFileSyncClient::DidUploadFile,
                 AsWeakPtr(), callback),
      google_apis::UploaderReadyCallback());
}

void DriveFileSyncClient::UploadExistingFileInternal(
    const std::string& remote_file_md5,
    const FilePath& local_file_path,
    int64 file_size,
    const UploadFileCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::DocumentEntry> entry) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    callback.Run(error, std::string(), std::string());
    return;
  }
  DCHECK(entry);

  // If remote file's hash value is different from the expected one, conflict
  // might have occurred.
  if (remote_file_md5 != entry->file_md5()) {
    callback.Run(google_apis::HTTP_CONFLICT, std::string(), std::string());
    return;
  }

  std::string mime_type;
  if (!net::GetWellKnownMimeTypeFromExtension(
          local_file_path.Extension(), &mime_type))
    mime_type = kMimeTypeOctetStream;

  // TODO(nhiroki): support ETag. Currently we assume there is no change between
  // GetDocumentEntry and UploadExistingFile call.
  drive_uploader_->UploadExistingFile(
      entry->GetLinkByType(
          google_apis::Link::LINK_RESUMABLE_EDIT_MEDIA)->href(),
      FilePath(kDummyDrivePath),
      local_file_path,
      mime_type,
      file_size,
      base::Bind(&DriveFileSyncClient::DidUploadFile,
                 AsWeakPtr(), callback),
      google_apis::UploaderReadyCallback());
}

void DriveFileSyncClient::DidUploadFile(
    const UploadFileCallback& callback,
    google_apis::DriveUploadError error,
    const FilePath& drive_path,
    const FilePath& file_path,
    scoped_ptr<google_apis::DocumentEntry> entry) {
  DCHECK(CalledOnValidThread());

  // Convert DriveUploadError to GDataErrorCode.
  switch (error) {
    case google_apis::DRIVE_UPLOAD_OK:
      DCHECK(entry);
      callback.Run(google_apis::HTTP_SUCCESS,
                   entry->resource_id(), entry->file_md5());
      return;
    case google_apis::DRIVE_UPLOAD_ERROR_NOT_FOUND:
      callback.Run(google_apis::HTTP_NOT_FOUND,
                   std::string(), std::string());
      return;
    case google_apis::DRIVE_UPLOAD_ERROR_NO_SPACE:
      callback.Run(google_apis::GDATA_NO_SPACE,
                   std::string(), std::string());
      return;
    case google_apis::DRIVE_UPLOAD_ERROR_ABORT:
      callback.Run(google_apis::GDATA_OTHER_ERROR,
                   std::string(), std::string());
      return;
  }
  NOTREACHED();
  callback.Run(google_apis::GDATA_OTHER_ERROR, std::string(), std::string());
}

void DriveFileSyncClient::DeleteFileInternal(
    const std::string& remote_file_md5,
    const GDataErrorCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::DocumentEntry> entry) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    callback.Run(error);
    return;
  }
  DCHECK(entry);

  // If remote file's hash value is different from the expected one, conflict
  // might have occurred.
  if (remote_file_md5 != entry->file_md5()) {
    callback.Run(google_apis::HTTP_CONFLICT);
    return;
  }

  // Move the file to trash (don't delete it completely).
  // TODO(nhiroki): support ETag. Currently we assume there is no change between
  // GetDocumentEntry and DeleteFile call.
  drive_service_->DeleteDocument(
      GURL(entry->GetLinkByType(google_apis::Link::LINK_SELF)->href()),
      base::Bind(&DriveFileSyncClient::DidDeleteFile,
                 AsWeakPtr(), callback));
}

void DriveFileSyncClient::DidDeleteFile(
    const GDataErrorCallback& callback,
    google_apis::GDataErrorCode error) {
  DCHECK(CalledOnValidThread());
  callback.Run(error);
}

}  // namespace sync_file_system
