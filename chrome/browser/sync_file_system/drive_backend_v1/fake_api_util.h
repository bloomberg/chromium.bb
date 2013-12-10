// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_FAKE_API_UTIL_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_FAKE_API_UTIL_H_

#include <map>
#include <string>

#include "chrome/browser/sync_file_system/drive_backend_v1/api_util_interface.h"
#include "chrome/browser/sync_file_system/sync_file_type.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/gdata_wapi_parser.h"
#include "google_apis/drive/gdata_wapi_url_generator.h"

class GURL;
class Profile;

namespace google_apis {
class ResourceEntry;
}

namespace sync_file_system {
namespace drive_backend {

class FakeAPIUtil : public APIUtilInterface {
 public:
  struct RemoteResource {
    std::string parent_resource_id;
    std::string parent_title;
    std::string title;
    std::string resource_id;
    std::string md5_checksum;
    SyncFileType type;
    bool deleted;
    int64 changestamp;

    RemoteResource();
    RemoteResource(const std::string& parent_resource_id,
                   const std::string& parent_title,
                   const std::string& title,
                   const std::string& resource_id,
                   const std::string& md5_checksum,
                   SyncFileType type,
                   bool deleted,
                   int64 changestamp);
    ~RemoteResource();
  };

  struct RemoteResourceComparator {
    // Returns lexicographical order referring all members.
    bool operator()(const RemoteResource& left, const RemoteResource& right);
  };

  typedef std::map<std::string, RemoteResource> RemoteResourceByResourceId;

  FakeAPIUtil();
  virtual ~FakeAPIUtil();

  // APIUtilInterface overrides.
  virtual void AddObserver(APIUtilObserver* observer) OVERRIDE;
  virtual void RemoveObserver(APIUtilObserver* observer) OVERRIDE;
  virtual void GetDriveDirectoryForSyncRoot(
      const ResourceIdCallback& callback) OVERRIDE;
  virtual void GetDriveDirectoryForOrigin(
      const std::string& sync_root_resource_id,
      const GURL& origin,
      const ResourceIdCallback& callback) OVERRIDE;
  virtual void GetLargestChangeStamp(
      const ChangeStampCallback& callback) OVERRIDE;
  virtual void GetResourceEntry(const std::string& resource_id,
                                const ResourceEntryCallback& callback) OVERRIDE;
  virtual void ListFiles(const std::string& directory_resource_id,
                         const ResourceListCallback& callback) OVERRIDE;
  virtual void ListChanges(int64 start_changestamp,
                           const ResourceListCallback& callback) OVERRIDE;
  virtual void ContinueListing(const GURL& next_link,
                               const ResourceListCallback& callback) OVERRIDE;
  virtual void DownloadFile(const std::string& resource_id,
                            const std::string& local_file_md5,
                            const DownloadFileCallback& callback) OVERRIDE;
  virtual void UploadNewFile(const std::string& directory_resource_id,
                             const base::FilePath& local_file_path,
                             const std::string& title,
                             const UploadFileCallback& callback) OVERRIDE;
  virtual void UploadExistingFile(const std::string& resource_id,
                                  const std::string& remote_file_md5,
                                  const base::FilePath& local_file_path,
                                  const UploadFileCallback& callback) OVERRIDE;
  virtual void CreateDirectory(const std::string& parent_resource_id,
                               const std::string& title,
                               const ResourceIdCallback& callback) OVERRIDE;
  virtual bool IsAuthenticated() const OVERRIDE;
  virtual void DeleteFile(const std::string& resource_id,
                          const std::string& remote_file_md5,
                          const GDataErrorCallback& callback) OVERRIDE;
  virtual void EnsureSyncRootIsNotInMyDrive(
      const std::string& sync_root_resource_id) OVERRIDE;

  void PushRemoteChange(const std::string& parent_resource_id,
                        const std::string& parent_title,
                        const std::string& title,
                        const std::string& resource_id,
                        const std::string& md5,
                        SyncFileType type,
                        bool deleted);

  const RemoteResourceByResourceId& remote_resources() const {
    return remote_resources_;
  }

 private:
  struct ChangeStampComparator;
  RemoteResourceByResourceId remote_resources_;

  scoped_ptr<google_apis::ResourceEntry> CreateResourceEntry(
      const RemoteResource& resource_id) const;
  GURL ResourceIdToResourceLink(const std::string& resource_id) const;

  int64 largest_changestamp_;
  google_apis::GDataWapiUrlGenerator url_generator_;

  DISALLOW_COPY_AND_ASSIGN(FakeAPIUtil);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_FAKE_API_UTIL_H_
