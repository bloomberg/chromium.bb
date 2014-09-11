// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides Drive specific API functions.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_DRIVE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_DRIVE_H_

#include "base/files/file.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_base.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"

namespace drive {
class FileCacheEntry;
class ResourceEntry;
struct SearchResultInfo;
}

namespace google_apis {
class AuthService;
}

namespace extensions {

namespace api {
namespace file_manager_private {
struct EntryProperties;
}  // namespace file_manager_private
}  // namespace api

// Retrieves property information for an entry and returns it as a dictionary.
// On error, returns a dictionary with the key "error" set to the error number
// (base::File::Error).
class FileManagerPrivateGetEntryPropertiesFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getEntryProperties",
                             FILEMANAGERPRIVATE_GETENTRYPROPERTIES)

  FileManagerPrivateGetEntryPropertiesFunction();

 protected:
  virtual ~FileManagerPrivateGetEntryPropertiesFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
  void CompleteGetEntryProperties(
      size_t index,
      scoped_ptr<api::file_manager_private::EntryProperties> properties,
      base::File::Error error);

  size_t processed_count_;
  std::vector<linked_ptr<api::file_manager_private::EntryProperties> >
      properties_list_;
};

// Implements the chrome.fileManagerPrivate.pinDriveFile method.
class FileManagerPrivatePinDriveFileFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.pinDriveFile",
                             FILEMANAGERPRIVATE_PINDRIVEFILE)

 protected:
  virtual ~FileManagerPrivatePinDriveFileFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
  // Callback for RunAsync().
  void OnPinStateSet(drive::FileError error);
};

// Implements the chrome.fileManagerPrivate.cancelFileTransfers method.
class FileManagerPrivateCancelFileTransfersFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.cancelFileTransfers",
                             FILEMANAGERPRIVATE_CANCELFILETRANSFERS)

 protected:
  virtual ~FileManagerPrivateCancelFileTransfersFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;
};

class FileManagerPrivateSearchDriveFunction
    : public LoggedAsyncExtensionFunction {
 public:
  typedef std::vector<drive::SearchResultInfo> SearchResultInfoList;

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.searchDrive",
                             FILEMANAGERPRIVATE_SEARCHDRIVE)

 protected:
  virtual ~FileManagerPrivateSearchDriveFunction() {}

  virtual bool RunAsync() OVERRIDE;

 private:
  // Callback for Search().
  void OnSearch(drive::FileError error,
                const GURL& next_link,
                scoped_ptr<std::vector<drive::SearchResultInfo> > result_paths);

  // Called when |result_paths| in OnSearch() are converted to a list of
  // entry definitions.
  void OnEntryDefinitionList(
      const GURL& next_link,
      scoped_ptr<SearchResultInfoList> search_result_info_list,
      scoped_ptr<file_manager::util::EntryDefinitionList>
          entry_definition_list);
};

// Similar to FileManagerPrivateSearchDriveFunction but this one is used for
// searching drive metadata which is stored locally.
class FileManagerPrivateSearchDriveMetadataFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.searchDriveMetadata",
                             FILEMANAGERPRIVATE_SEARCHDRIVEMETADATA)

 protected:
  virtual ~FileManagerPrivateSearchDriveMetadataFunction() {}

  virtual bool RunAsync() OVERRIDE;

 private:
  // Callback for SearchMetadata();
  void OnSearchMetadata(drive::FileError error,
                        scoped_ptr<drive::MetadataSearchResultVector> results);

  // Called when |results| in OnSearchMetadata() are converted to a list of
  // entry definitions.
  void OnEntryDefinitionList(
      scoped_ptr<drive::MetadataSearchResultVector> search_result_info_list,
      scoped_ptr<file_manager::util::EntryDefinitionList>
          entry_definition_list);
};

// Implements the chrome.fileManagerPrivate.getDriveConnectionState method.
class FileManagerPrivateGetDriveConnectionStateFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileManagerPrivate.getDriveConnectionState",
      FILEMANAGERPRIVATE_GETDRIVECONNECTIONSTATE);

 protected:
  virtual ~FileManagerPrivateGetDriveConnectionStateFunction() {}

  virtual bool RunSync() OVERRIDE;
};

// Implements the chrome.fileManagerPrivate.requestAccessToken method.
class FileManagerPrivateRequestAccessTokenFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.requestAccessToken",
                             FILEMANAGERPRIVATE_REQUESTACCESSTOKEN)

 protected:
  virtual ~FileManagerPrivateRequestAccessTokenFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

  // Callback with a cached auth token (if available) or a fetched one.
  void OnAccessTokenFetched(google_apis::GDataErrorCode code,
                            const std::string& access_token);
};

// Implements the chrome.fileManagerPrivate.getShareUrl method.
class FileManagerPrivateGetShareUrlFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getShareUrl",
                             FILEMANAGERPRIVATE_GETSHAREURL)

 protected:
  virtual ~FileManagerPrivateGetShareUrlFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

  // Callback with an url to the sharing dialog as |share_url|, called by
  // FileSystem::GetShareUrl.
  void OnGetShareUrl(drive::FileError error, const GURL& share_url);
};

// Implements the chrome.fileManagerPrivate.requestDriveShare method.
class FileManagerPrivateRequestDriveShareFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.requestDriveShare",
                             FILEMANAGERPRIVATE_REQUESTDRIVESHARE);

 protected:
  virtual ~FileManagerPrivateRequestDriveShareFunction() {}
  virtual bool RunAsync() OVERRIDE;

 private:
  // Called back after the drive file system operation is finished.
  void OnAddPermission(drive::FileError error);
};

// Implements the chrome.fileManagerPrivate.getDownloadUrl method.
class FileManagerPrivateGetDownloadUrlFunction
    : public LoggedAsyncExtensionFunction {
 public:
  FileManagerPrivateGetDownloadUrlFunction();

  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getDownloadUrl",
                             FILEMANAGERPRIVATE_GETDOWNLOADURL)

 protected:
  virtual ~FileManagerPrivateGetDownloadUrlFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

  void OnGetResourceEntry(drive::FileError error,
                          scoped_ptr<drive::ResourceEntry> entry);

  // Callback with an |access_token|, called by
  // drive::DriveReadonlyTokenFetcher.
  void OnTokenFetched(google_apis::GDataErrorCode code,
                      const std::string& access_token);

 private:
  std::string download_url_;
  scoped_ptr<google_apis::AuthService> auth_service_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_DRIVE_H_
