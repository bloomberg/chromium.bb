// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_file_sync_client.h"

#include <algorithm>
#include <functional>
#include <sstream>
#include <string>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/google_apis/drive_uploader.h"
#include "chrome/browser/google_apis/gdata_wapi_service.h"
#include "chrome/browser/google_apis/gdata_wapi_url_generator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/drive_file_sync_util.h"
#include "chrome/common/extensions/extension.h"
#include "extensions/common/constants.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"

namespace sync_file_system {

namespace {

const char kRootResourceId[] = "";
const char kSyncRootDirectoryName[] = "Chrome Syncable FileSystem";
const char kMimeTypeOctetStream[] = "application/octet-stream";

// This path is not actually used but is required by DriveUploaderInterface.
const base::FilePath::CharType kDummyDrivePath[] =
    FILE_PATH_LITERAL("/dummy/drive/path");

bool HasParentLinkTo(const ScopedVector<google_apis::Link>& links,
                     const GURL& parent_link) {
  bool should_not_have_parent = parent_link.is_empty();

  for (ScopedVector<google_apis::Link>::const_iterator itr = links.begin();
       itr != links.end(); ++itr) {
    if ((*itr)->type() == google_apis::Link::LINK_PARENT) {
      if (should_not_have_parent)
        return false;
      if ((*itr)->href().GetOrigin() == parent_link.GetOrigin() &&
          (*itr)->href().path() == parent_link.path())
        return true;
    }
  }

  return should_not_have_parent;
}

struct TitleAndParentQuery
    : std::unary_function<const google_apis::ResourceEntry*, bool> {
  TitleAndParentQuery(const std::string& title,
                      const GURL& parent_link)
      : title(title),
        parent_link(parent_link) {
  }

  bool operator()(const google_apis::ResourceEntry* entry) const {
    return entry->title() == title &&
        HasParentLinkTo(entry->links(), parent_link);
  }

  const std::string& title;
  const GURL& parent_link;
};

void FilterEntriesByTitleAndParent(
    ScopedVector<google_apis::ResourceEntry>* entries,
    const std::string& title,
    const GURL& parent_link) {
  typedef ScopedVector<google_apis::ResourceEntry>::iterator iterator;
  iterator itr = std::partition(entries->begin(), entries->end(),
                                TitleAndParentQuery(title, parent_link));
  entries->erase(itr, entries->end());
}

google_apis::ResourceEntry* GetDocumentByTitleAndParent(
    const ScopedVector<google_apis::ResourceEntry>& entries,
    const std::string& title,
    const GURL& parent_link) {
  typedef ScopedVector<google_apis::ResourceEntry>::const_iterator iterator;
  iterator found = std::find_if(entries.begin(), entries.end(),
                                TitleAndParentQuery(title, parent_link));
  if (found != entries.end())
    return *found;
  return NULL;
}

void EntryAdapter(scoped_ptr<google_apis::ResourceEntry> entry,
                  const DriveFileSyncClient::ResourceEntryCallback& callback,
                  google_apis::GDataErrorCode error) {
  callback.Run(error, entry.Pass());
}

void UploadResultAdapter(
    const DriveFileSyncClient::ResourceEntryCallback& callback,
    google_apis::DriveUploadError error,
    const base::FilePath& drive_path,
    const base::FilePath& file_path,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  callback.Run(DriveUploadErrorToGDataErrorCode(error), entry.Pass());
}

}  // namespace

DriveFileSyncClient::DriveFileSyncClient(Profile* profile)
    : url_generator_(GURL(
          google_apis::GDataWapiUrlGenerator::kBaseUrlForProduction)) {
  drive_service_.reset(new google_apis::GDataWapiService(
      profile->GetRequestContext(),
      GURL(google_apis::GDataWapiUrlGenerator::kBaseUrlForProduction),
      "" /* custom_user_agent */));
  drive_service_->Initialize(profile);
  drive_service_->AddObserver(this);
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);

  drive_uploader_.reset(new google_apis::DriveUploader(drive_service_.get()));
}

scoped_ptr<DriveFileSyncClient> DriveFileSyncClient::CreateForTesting(
    Profile* profile,
    const GURL& base_url,
    scoped_ptr<google_apis::DriveServiceInterface> drive_service,
    scoped_ptr<google_apis::DriveUploaderInterface> drive_uploader) {
  return make_scoped_ptr(new DriveFileSyncClient(
      profile, base_url, drive_service.Pass(), drive_uploader.Pass()));
}

DriveFileSyncClient::DriveFileSyncClient(
    Profile* profile,
    const GURL& base_url,
    scoped_ptr<google_apis::DriveServiceInterface> drive_service,
    scoped_ptr<google_apis::DriveUploaderInterface> drive_uploader)
    : url_generator_(base_url) {
  drive_service_ = drive_service.Pass();
  drive_service_->Initialize(profile);
  drive_service_->AddObserver(this);
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);

  drive_uploader_ = drive_uploader.Pass();
}

DriveFileSyncClient::~DriveFileSyncClient() {
  DCHECK(CalledOnValidThread());
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
  drive_service_->RemoveObserver(this);
  drive_service_->CancelAll();
}

void DriveFileSyncClient::AddObserver(DriveFileSyncClientObserver* observer) {
  DCHECK(CalledOnValidThread());
  observers_.AddObserver(observer);
}

void DriveFileSyncClient::RemoveObserver(
    DriveFileSyncClientObserver* observer) {
  DCHECK(CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void DriveFileSyncClient::GetDriveDirectoryForSyncRoot(
    const ResourceIdCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Getting Drive directory for SyncRoot";

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
  DVLOG(2) << "Getting Drive directory for Origin: " << origin;

  std::string directory_name(OriginToDirectoryTitle(origin));
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
    scoped_ptr<google_apis::ResourceList> feed) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsStringASCII(directory_name));

  if (error != google_apis::HTTP_SUCCESS) {
    DVLOG(2) << "Error on getting Drive directory: " << error;
    callback.Run(error, std::string());
    return;
  }

  GURL parent_link;
  if (!parent_resource_id.empty())
    parent_link = ResourceIdToResourceLink(parent_resource_id);
  std::string title(directory_name);
  google_apis::ResourceEntry* entry = GetDocumentByTitleAndParent(
      feed->entries(), title, parent_link);
  if (!entry) {
    DVLOG(2) << "Directory not found. Creating: " << directory_name;

    // If the |parent_resource_id| is empty, create a directory under the root
    // directory. So here we use the result of GetRootResourceId() for such a
    // case.
    std::string resource_id = parent_resource_id.empty() ?
        drive_service_->GetRootResourceId() : parent_resource_id;
    drive_service_->AddNewDirectory(
        resource_id, directory_name,
        base::Bind(&DriveFileSyncClient::DidCreateDirectory, AsWeakPtr(),
                   parent_resource_id, title, callback));
    return;
  }
  DVLOG(2) << "Found Drive directory.";

  // TODO(tzik): Handle error.
  DCHECK_EQ(google_apis::ENTRY_KIND_FOLDER, entry->kind());
  DCHECK_EQ(directory_name, entry->title());

  callback.Run(error, entry->resource_id());
}

void DriveFileSyncClient::DidCreateDirectory(
    const std::string& parent_resource_id,
    const std::string& title,
    const ResourceIdCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS &&
      error != google_apis::HTTP_CREATED) {
    DVLOG(2) << "Error on creating Drive directory: " << error;
    callback.Run(error, std::string());
    return;
  }
  DVLOG(2) << "Created Drive directory.";

  DCHECK(entry);
  // Check if any other client creates a directory with same title.
  EnsureTitleUniqueness(
      parent_resource_id, title,
      base::Bind(&DriveFileSyncClient::DidEnsureUniquenessForCreateDirectory,
                 AsWeakPtr(), callback));
}

void DriveFileSyncClient::DidEnsureUniquenessForCreateDirectory(
    const ResourceIdCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  DCHECK(CalledOnValidThread());
  // If error == HTTP_FOUND: the directory is successfully created without
  //   conflict.
  // If error == HTTP_SUCCESS: the directory is created with conflict, but
  //   the conflict was resolved.
  //

  if (error == google_apis::HTTP_FOUND) {
    error = google_apis::HTTP_CREATED;
  }
  callback.Run(error, entry->resource_id());
}

void DriveFileSyncClient::GetLargestChangeStamp(
    const ChangeStampCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Getting largest changestamp";

  drive_service_->GetAccountMetadata(
      base::Bind(&DriveFileSyncClient::DidGetAccountMetadata,
                 AsWeakPtr(), callback));
}

void DriveFileSyncClient::GetResourceEntry(
    const std::string& resource_id,
    const ResourceEntryCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Getting ResourceEntry for: " << resource_id;

  drive_service_->GetResourceEntry(
      resource_id,
      base::Bind(&DriveFileSyncClient::DidGetResourceEntry,
                 AsWeakPtr(), callback));
}

void DriveFileSyncClient::DidGetAccountMetadata(
    const ChangeStampCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::AccountMetadataFeed> metadata) {
  DCHECK(CalledOnValidThread());

  int64 largest_changestamp = 0;
  if (error == google_apis::HTTP_SUCCESS) {
    DCHECK(metadata);
    largest_changestamp = metadata->largest_changestamp();
    DVLOG(2) << "Got largest changestamp: " << largest_changestamp;
  } else {
    DVLOG(2) << "Error on getting largest changestamp: " << error;
  }

  callback.Run(error, largest_changestamp);
}

void DriveFileSyncClient::SearchFilesInDirectory(
    const std::string& directory_resource_id,
    const std::string& search_query,
    const ResourceListCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Searching resources in the directory [" << directory_resource_id
           << "] with query [" << search_query << "]";

  drive_service_->GetResourceList(
      GURL(),  // feed_url
      0,  // start_changestamp
      search_query,
      false,  // shared_with_me
      directory_resource_id,
      base::Bind(&DriveFileSyncClient::DidGetResourceList,
                 AsWeakPtr(), callback));
}

void DriveFileSyncClient::ListFiles(const std::string& directory_resource_id,
                                    const ResourceListCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Listing resources in the directory ["
           << directory_resource_id << "]";

  SearchFilesInDirectory(directory_resource_id,
                         std::string() /* search_query */,
                         callback);
}

void DriveFileSyncClient::ListChanges(int64 start_changestamp,
                                      const ResourceListCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Listing changes since: " << start_changestamp;

  drive_service_->GetResourceList(
      GURL(),  // feed_url
      start_changestamp,
      std::string(),  // search_query
      false,  // shared_with_me
      std::string(),  // directory_resource_id
      base::Bind(&DriveFileSyncClient::DidGetResourceList,
                 AsWeakPtr(), callback));
}

void DriveFileSyncClient::ContinueListing(
    const GURL& feed_url,
    const ResourceListCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Continue listing on feed: " << feed_url;

  drive_service_->GetResourceList(
      feed_url,
      0,  // start_changestamp
      std::string(),  // search_query
      false,  // shared_with_me
      std::string(),  // directory_resource_id
      base::Bind(&DriveFileSyncClient::DidGetResourceList,
                 AsWeakPtr(), callback));
}

void DriveFileSyncClient::DownloadFile(
    const std::string& resource_id,
    const std::string& local_file_md5,
    const base::FilePath& local_file_path,
    const DownloadFileCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Downloading file [" << resource_id << "]";

  drive_service_->GetResourceEntry(
      resource_id,
      base::Bind(&DriveFileSyncClient::DidGetResourceEntry,
                 AsWeakPtr(),
                 base::Bind(&DriveFileSyncClient::DownloadFileInternal,
                            AsWeakPtr(), local_file_md5, local_file_path,
                            callback)));
}

void DriveFileSyncClient::UploadNewFile(
    const std::string& directory_resource_id,
    const base::FilePath& local_file_path,
    const std::string& title,
    const UploadFileCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Uploading new file into the directory ["
           << directory_resource_id << "] with title ["
           << title << "]";

  std::string mime_type;
  if (!net::GetWellKnownMimeTypeFromExtension(
          local_file_path.Extension(), &mime_type))
    mime_type = kMimeTypeOctetStream;

  ResourceEntryCallback did_upload_callback =
      base::Bind(&DriveFileSyncClient::DidUploadNewFile, AsWeakPtr(),
                 directory_resource_id, title, callback);
  drive_uploader_->UploadNewFile(
      directory_resource_id,
      base::FilePath(kDummyDrivePath),
      local_file_path,
      title,
      mime_type,
      base::Bind(&UploadResultAdapter, did_upload_callback));
}

void DriveFileSyncClient::UploadExistingFile(
    const std::string& resource_id,
    const std::string& remote_file_md5,
    const base::FilePath& local_file_path,
    const UploadFileCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Uploading existing file [" << resource_id << "]";
  drive_service_->GetResourceEntry(
      resource_id,
      base::Bind(&DriveFileSyncClient::DidGetResourceEntry,
                 AsWeakPtr(),
                 base::Bind(&DriveFileSyncClient::UploadExistingFileInternal,
                            AsWeakPtr(), remote_file_md5, local_file_path,
                            callback)));
}

void DriveFileSyncClient::DeleteFile(
    const std::string& resource_id,
    const std::string& remote_file_md5,
    const GDataErrorCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Deleting file: " << resource_id;

  drive_service_->GetResourceEntry(
      resource_id,
      base::Bind(&DriveFileSyncClient::DidGetResourceEntry,
                 AsWeakPtr(),
                 base::Bind(&DriveFileSyncClient::DeleteFileInternal,
                            AsWeakPtr(), remote_file_md5, callback)));
}

GURL DriveFileSyncClient::ResourceIdToResourceLink(
    const std::string& resource_id) const {
  return url_generator_.GenerateEditUrl(resource_id);
}

// static
std::string DriveFileSyncClient::OriginToDirectoryTitle(const GURL& origin) {
  DCHECK(origin.SchemeIs(extensions::kExtensionScheme));
  return origin.host();
}

// static
GURL DriveFileSyncClient::DirectoryTitleToOrigin(const std::string& title) {
  return extensions::Extension::GetBaseURLFromExtensionId(title);
}

void DriveFileSyncClient::OnReadyToPerformOperations() {
  DCHECK(CalledOnValidThread());
  FOR_EACH_OBSERVER(DriveFileSyncClientObserver, observers_, OnAuthenticated());
}

void DriveFileSyncClient::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DCHECK(CalledOnValidThread());
  if (type != net::NetworkChangeNotifier::CONNECTION_NONE)
    FOR_EACH_OBSERVER(DriveFileSyncClientObserver,
                      observers_, OnNetworkConnected());
}

void DriveFileSyncClient::DidGetResourceList(
    const ResourceListCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceList> resource_list) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    DVLOG(2) << "Error on listing resource: " << error;
    callback.Run(error, scoped_ptr<google_apis::ResourceList>());
    return;
  }

  DVLOG(2) << "Got resource list";
  DCHECK(resource_list);
  callback.Run(error, resource_list.Pass());
}

void DriveFileSyncClient::DidGetResourceEntry(
    const ResourceEntryCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    DVLOG(2) << "Error on getting resource entry:" << error;
    callback.Run(error, scoped_ptr<google_apis::ResourceEntry>());
    return;
  }

  DVLOG(2) << "Got resource entry";
  DCHECK(entry);
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
    const base::FilePath& local_file_path,
    const DownloadFileCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    DVLOG(2) << "Error on getting resource entry for download";
    callback.Run(error, std::string());
    return;
  }
  DCHECK(entry);

  DVLOG(2) << "Got resource entry for download";
  // If local file and remote file are same, cancel the download.
  if (local_file_md5 == entry->file_md5()) {
    callback.Run(google_apis::HTTP_NOT_MODIFIED, local_file_md5);
    return;
  }

  DVLOG(2) << "Downloading file: " << entry->resource_id();
  drive_service_->DownloadFile(
      base::FilePath(kDummyDrivePath),
      local_file_path,
      entry->download_url(),
      base::Bind(&DriveFileSyncClient::DidDownloadFile,
                 AsWeakPtr(), entry->file_md5(), callback),
      google_apis::GetContentCallback());
}

void DriveFileSyncClient::DidDownloadFile(
    const std::string& downloaded_file_md5,
    const DownloadFileCallback& callback,
    google_apis::GDataErrorCode error,
    const base::FilePath& downloaded_file_path) {
  DCHECK(CalledOnValidThread());
  if (error == google_apis::HTTP_SUCCESS)
    DVLOG(2) << "Download completed";
  else
    DVLOG(2) << "Error on downloading file: " << error;

  callback.Run(error, downloaded_file_md5);
}

void DriveFileSyncClient::DidUploadNewFile(
    const std::string& parent_resource_id,
    const std::string& title,
    const UploadFileCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  if (error != google_apis::HTTP_SUCCESS &&
      error != google_apis::HTTP_CREATED) {
    DVLOG(2) << "Error on uploading new file: " << error;
    callback.Run(error, std::string(), std::string());
    return;
  }

  DVLOG(2) << "Upload completed";
  EnsureTitleUniqueness(
      parent_resource_id, title,
      base::Bind(&DriveFileSyncClient::DidEnsureUniquenessForCreateFile,
                 AsWeakPtr(), entry->resource_id(), callback));
}

void DriveFileSyncClient::DidEnsureUniquenessForCreateFile(
    const std::string& expected_resource_id,
    const UploadFileCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  if (error == google_apis::HTTP_FOUND) {
    // The file was uploaded successfully and no conflict was detected.
    DCHECK(entry);
    DVLOG(2) << "No conflict detected on uploading new file";
    callback.Run(google_apis::HTTP_CREATED,
                 entry->resource_id(), entry->file_md5());
    return;
  }

  if (error == google_apis::HTTP_SUCCESS) {
    // The file was uploaded successfully but a conflict was detected.
    // The duplicated file was deleted successfully.
    DCHECK(entry);
    if (entry->resource_id() != expected_resource_id) {
      DVLOG(2) << "Conflict detected on uploading new file";
      callback.Run(google_apis::HTTP_CONFLICT,
                   std::string(), std::string());
      return;
    }

    DVLOG(2) << "Conflict detected on uploading new file and resolved";
    callback.Run(google_apis::HTTP_CREATED,
                 entry->resource_id(), entry->file_md5());
    return;
  }

  DVLOG(2) << "Error on uploading new file: " << error;
  callback.Run(error, std::string(), std::string());
}

void DriveFileSyncClient::UploadExistingFileInternal(
    const std::string& remote_file_md5,
    const base::FilePath& local_file_path,
    const UploadFileCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    DVLOG(2) << "Error on uploading existing file: " << error;
    callback.Run(error, std::string(), std::string());
    return;
  }
  DCHECK(entry);

  // If remote file's hash value is different from the expected one, conflict
  // might have occurred.
  if (remote_file_md5 != entry->file_md5()) {
    DVLOG(2) << "Conflict detected before uploading existing file";
    callback.Run(google_apis::HTTP_CONFLICT, std::string(), std::string());
    return;
  }

  std::string mime_type;
  if (!net::GetWellKnownMimeTypeFromExtension(
          local_file_path.Extension(), &mime_type))
    mime_type = kMimeTypeOctetStream;

  ResourceEntryCallback did_upload_callback =
      base::Bind(&DriveFileSyncClient::DidUploadExistingFile,
                 AsWeakPtr(), callback);
  drive_uploader_->UploadExistingFile(
      entry->resource_id(),
      base::FilePath(kDummyDrivePath),
      local_file_path,
      mime_type,
      entry->etag(),
      base::Bind(&UploadResultAdapter, did_upload_callback));
}

bool DriveFileSyncClient::IsAuthenticated() const {
  return drive_service_->HasRefreshToken();
}

void DriveFileSyncClient::DidUploadExistingFile(
    const UploadFileCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  DCHECK(CalledOnValidThread());
  if (error != google_apis::HTTP_SUCCESS) {
    DVLOG(2) << "Error on uploading existing file: " << error;
    callback.Run(error, std::string(), std::string());
    return;
  }

  DCHECK(entry);
  DVLOG(2) << "Upload completed";
  callback.Run(error, entry->resource_id(), entry->file_md5());
}

void DriveFileSyncClient::DeleteFileInternal(
    const std::string& remote_file_md5,
    const GDataErrorCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS) {
    DVLOG(2) << "Error on getting resource entry for deleting file: " << error;
    callback.Run(error);
    return;
  }
  DCHECK(entry);

  // If remote file's hash value is different from the expected one, conflict
  // might have occurred.
  if (remote_file_md5 != entry->file_md5()) {
    DVLOG(2) << "Conflict detected before deleting file";
    callback.Run(google_apis::HTTP_CONFLICT);
    return;
  }
  DVLOG(2) << "Got resource entry for deleting file";

  // Move the file to trash (don't delete it completely).
  drive_service_->DeleteResource(
      entry->resource_id(),
      entry->etag(),
      base::Bind(&DriveFileSyncClient::DidDeleteFile,
                 AsWeakPtr(), callback));
}

void DriveFileSyncClient::DidDeleteFile(
    const GDataErrorCallback& callback,
    google_apis::GDataErrorCode error) {
  DCHECK(CalledOnValidThread());
  if (error == google_apis::HTTP_SUCCESS)
    DVLOG(2) << "Deletion completed";
  else
    DVLOG(2) << "Error on deleting file: " << error;

  callback.Run(error);
}

void DriveFileSyncClient::EnsureTitleUniqueness(
    const std::string& parent_resource_id,
    const std::string& expected_title,
    const ResourceEntryCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Checking if there's no conflict on entry creation";

  SearchFilesInDirectory(
      parent_resource_id,
      FormatTitleQuery(expected_title),
      base::Bind(&DriveFileSyncClient::DidListEntriesToEnsureUniqueness,
                 AsWeakPtr(), parent_resource_id, expected_title, callback));
}

void DriveFileSyncClient::DidListEntriesToEnsureUniqueness(
    const std::string& parent_resource_id,
    const std::string& expected_title,
    const ResourceEntryCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceList> feed) {
  DCHECK(CalledOnValidThread());
  if (error != google_apis::HTTP_SUCCESS) {
    DVLOG(2) << "Error on listing resource for ensuring title uniqueness";
    callback.Run(error, scoped_ptr<google_apis::ResourceEntry>());
    return;
  }
  DVLOG(2) << "Got resource list for ensuring title uniqueness";

  // This filtering is needed only on WAPI. Once we move to Drive API we can
  // drop this.
  GURL parent_link;
  if (!parent_resource_id.empty())
    parent_link = ResourceIdToResourceLink(parent_resource_id);
  ScopedVector<google_apis::ResourceEntry> entries;
  entries.swap(*feed->mutable_entries());
  FilterEntriesByTitleAndParent(&entries, expected_title, parent_link);

  if (entries.empty()) {
    DVLOG(2) << "Uploaded file is not found";
    callback.Run(google_apis::HTTP_NOT_FOUND,
                 scoped_ptr<google_apis::ResourceEntry>());
    return;
  }

  if (entries.size() >= 2) {
    DVLOG(2) << "Conflict detected on creating entry";
    for (size_t i = 0; i < entries.size() - 1; ++i) {
      // TODO(tzik): Replace published_time with creation time after we move to
      // Drive API.
      if (entries[i]->published_time() < entries.back()->published_time())
        std::swap(entries[i], entries.back());
    }

    scoped_ptr<google_apis::ResourceEntry> earliest_entry(entries.back());
    entries.back() = NULL;
    entries.get().pop_back();

    DeleteEntriesForEnsuringTitleUniqueness(
        entries.Pass(),
        base::Bind(&EntryAdapter, base::Passed(&earliest_entry), callback));
    return;
  }

  DVLOG(2) << "no conflict detected";
  DCHECK_EQ(1u, entries.size());
  scoped_ptr<google_apis::ResourceEntry> entry(entries.front());
  entries.weak_clear();
  callback.Run(google_apis::HTTP_FOUND, entry.Pass());
}

void DriveFileSyncClient::DeleteEntriesForEnsuringTitleUniqueness(
    ScopedVector<google_apis::ResourceEntry> entries,
    const GDataErrorCallback& callback) {
  DCHECK(CalledOnValidThread());
  DVLOG(2) << "Cleaning up conflict on entry creation";

  if (entries.empty()) {
    callback.Run(google_apis::HTTP_SUCCESS);
    return;
  }

  scoped_ptr<google_apis::ResourceEntry> entry(entries.back());
  entries.back() = NULL;
  entries.get().pop_back();

  // We don't care conflicts here as other clients may be also deleting this
  // file, so passing an empty etag.
  drive_service_->DeleteResource(
      entry->resource_id(),
      std::string(),  // empty etag
      base::Bind(
          &DriveFileSyncClient::DidDeleteEntriesForEnsuringTitleUniqueness,
          AsWeakPtr(), base::Passed(&entries), callback));
}

void DriveFileSyncClient::DidDeleteEntriesForEnsuringTitleUniqueness(
    ScopedVector<google_apis::ResourceEntry> entries,
    const GDataErrorCallback& callback,
    google_apis::GDataErrorCode error) {
  DCHECK(CalledOnValidThread());

  if (error != google_apis::HTTP_SUCCESS &&
      error != google_apis::HTTP_NOT_FOUND) {
    DVLOG(2) << "Error on deleting file: " << error;
    callback.Run(error);
    return;
  }

  DVLOG(2) << "Deletion completed";
  DeleteEntriesForEnsuringTitleUniqueness(entries.Pass(), callback);
}

}  // namespace sync_file_system
