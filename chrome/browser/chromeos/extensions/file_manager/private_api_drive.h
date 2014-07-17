// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides Drive specific API functions.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_DRIVE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_DRIVE_H_

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
namespace file_browser_private {
struct DriveEntryProperties;
}  // namespace file_browser_private
}  // namespace api

// Retrieves property information for an entry and returns it as a dictionary.
// On error, returns a dictionary with the key "error" set to the error number
// (drive::FileError).
class FileBrowserPrivateGetDriveEntryPropertiesFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getDriveEntryProperties",
                             FILEBROWSERPRIVATE_GETDRIVEFILEPROPERTIES)

  FileBrowserPrivateGetDriveEntryPropertiesFunction();

 protected:
  virtual ~FileBrowserPrivateGetDriveEntryPropertiesFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
  void CompleteGetFileProperties(drive::FileError error);

  size_t processed_count_;
  std::vector<linked_ptr<api::file_browser_private::DriveEntryProperties> >
      properties_list_;
};

// Implements the chrome.fileBrowserPrivate.pinDriveFile method.
class FileBrowserPrivatePinDriveFileFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.pinDriveFile",
                             FILEBROWSERPRIVATE_PINDRIVEFILE)

 protected:
  virtual ~FileBrowserPrivatePinDriveFileFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
  // Callback for RunAsync().
  void OnPinStateSet(drive::FileError error);
};

// Get drive files for the given list of file URLs. Initiate downloading of
// drive files if these are not cached. Return a list of local file names.
// This function puts empty strings instead of local paths for files could
// not be obtained. For instance, this can happen if the user specifies a new
// file name to save a file on drive. There may be other reasons to fail. The
// file manager should check if the local paths returned from getDriveFiles()
// contain empty paths.
// TODO(satorux): Should we propagate error types to the JavaScript layer?
class FileBrowserPrivateGetDriveFilesFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getDriveFiles",
                             FILEBROWSERPRIVATE_GETDRIVEFILES)

  FileBrowserPrivateGetDriveFilesFunction();

 protected:
  virtual ~FileBrowserPrivateGetDriveFilesFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
  // Gets the file on the top of the |remaining_drive_paths_| or sends the
  // response if the queue is empty.
  void GetFileOrSendResponse();

  // Called by FileSystem::GetFile(). Pops the file from
  // |remaining_drive_paths_|, and calls GetFileOrSendResponse().
  void OnFileReady(drive::FileError error,
                   const base::FilePath& local_path,
                   scoped_ptr<drive::ResourceEntry> entry);

  std::queue<base::FilePath> remaining_drive_paths_;
  std::vector<std::string> local_paths_;
};

// Implements the chrome.fileBrowserPrivate.cancelFileTransfers method.
class FileBrowserPrivateCancelFileTransfersFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.cancelFileTransfers",
                             FILEBROWSERPRIVATE_CANCELFILETRANSFERS)

 protected:
  virtual ~FileBrowserPrivateCancelFileTransfersFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;
};

class FileBrowserPrivateSearchDriveFunction
    : public LoggedAsyncExtensionFunction {
 public:
  typedef std::vector<drive::SearchResultInfo> SearchResultInfoList;

  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.searchDrive",
                             FILEBROWSERPRIVATE_SEARCHDRIVE)

 protected:
  virtual ~FileBrowserPrivateSearchDriveFunction() {}

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

// Similar to FileBrowserPrivateSearchDriveFunction but this one is used for
// searching drive metadata which is stored locally.
class FileBrowserPrivateSearchDriveMetadataFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.searchDriveMetadata",
                             FILEBROWSERPRIVATE_SEARCHDRIVEMETADATA)

 protected:
  virtual ~FileBrowserPrivateSearchDriveMetadataFunction() {}

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

// Implements the chrome.fileBrowserPrivate.getDriveConnectionState method.
class FileBrowserPrivateGetDriveConnectionStateFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileBrowserPrivate.getDriveConnectionState",
      FILEBROWSERPRIVATE_GETDRIVECONNECTIONSTATE);

 protected:
  virtual ~FileBrowserPrivateGetDriveConnectionStateFunction() {}

  virtual bool RunSync() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.requestAccessToken method.
class FileBrowserPrivateRequestAccessTokenFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.requestAccessToken",
                             FILEBROWSERPRIVATE_REQUESTACCESSTOKEN)

 protected:
  virtual ~FileBrowserPrivateRequestAccessTokenFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

  // Callback with a cached auth token (if available) or a fetched one.
  void OnAccessTokenFetched(google_apis::GDataErrorCode code,
                            const std::string& access_token);
};

// Implements the chrome.fileBrowserPrivate.getShareUrl method.
class FileBrowserPrivateGetShareUrlFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getShareUrl",
                             FILEBROWSERPRIVATE_GETSHAREURL)

 protected:
  virtual ~FileBrowserPrivateGetShareUrlFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

  // Callback with an url to the sharing dialog as |share_url|, called by
  // FileSystem::GetShareUrl.
  void OnGetShareUrl(drive::FileError error, const GURL& share_url);
};

// Implements the chrome.fileBrowserPrivate.requestDriveShare method.
class FileBrowserPrivateRequestDriveShareFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.requestDriveShare",
                             FILEBROWSERPRIVATE_REQUESTDRIVESHARE);

 protected:
  virtual ~FileBrowserPrivateRequestDriveShareFunction() {}
  virtual bool RunAsync() OVERRIDE;

 private:
  // Called back after the drive file system operation is finished.
  void OnAddPermission(drive::FileError error);
};

// Implements the chrome.fileBrowserPrivate.getDownloadUrl method.
class FileBrowserPrivateGetDownloadUrlFunction
    : public LoggedAsyncExtensionFunction {
 public:
  FileBrowserPrivateGetDownloadUrlFunction();

  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getDownloadUrl",
                             FILEBROWSERPRIVATE_GETDOWNLOADURL)

 protected:
  virtual ~FileBrowserPrivateGetDownloadUrlFunction();

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
