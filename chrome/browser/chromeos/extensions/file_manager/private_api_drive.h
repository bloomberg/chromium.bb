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

namespace drive {
class FileCacheEntry;
class ResourceEntry;
struct DriveAppInfo;
struct SearchResultInfo;
}

namespace extensions {

// Retrieves property information for an entry and returns it as a dictionary.
// On error, returns a dictionary with the key "error" set to the error number
// (drive::FileError).
class GetDriveEntryPropertiesFunction : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getDriveEntryProperties",
                             FILEBROWSERPRIVATE_GETDRIVEFILEPROPERTIES)

  GetDriveEntryPropertiesFunction();

 protected:
  virtual ~GetDriveEntryPropertiesFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  void OnGetFileInfo(drive::FileError error,
                     scoped_ptr<drive::ResourceEntry> entry);

  void CacheStateReceived(bool success,
                          const drive::FileCacheEntry& cache_entry);

  void CompleteGetFileProperties(drive::FileError error);

  base::FilePath file_path_;
  scoped_ptr<base::DictionaryValue> properties_;
};

// Implements the chrome.fileBrowserPrivate.pinDriveFile method.
class PinDriveFileFunction : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.pinDriveFile",
                             FILEBROWSERPRIVATE_PINDRIVEFILE)

  PinDriveFileFunction();

 protected:
  virtual ~PinDriveFileFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // Callback for RunImpl().
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
class GetDriveFilesFunction : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getDriveFiles",
                             FILEBROWSERPRIVATE_GETDRIVEFILES)

  GetDriveFilesFunction();

 protected:
  virtual ~GetDriveFilesFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

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
  ListValue* local_paths_;
};

// Implements the chrome.fileBrowserPrivate.cancelFileTransfers method.
class CancelFileTransfersFunction : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.cancelFileTransfers",
                             FILEBROWSERPRIVATE_CANCELFILETRANSFERS)

  CancelFileTransfersFunction();

 protected:
  virtual ~CancelFileTransfersFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

class SearchDriveFunction : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.searchDrive",
                             FILEBROWSERPRIVATE_SEARCHDRIVE)

  SearchDriveFunction();

 protected:
  virtual ~SearchDriveFunction();

  virtual bool RunImpl() OVERRIDE;

 private:
  // Callback for Search().
  void OnSearch(drive::FileError error,
                const std::string& next_feed,
                scoped_ptr<std::vector<drive::SearchResultInfo> > result_paths);
};

// Similar to SearchDriveFunction but this one is used for searching drive
// metadata which is stored locally.
class SearchDriveMetadataFunction : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.searchDriveMetadata",
                             FILEBROWSERPRIVATE_SEARCHDRIVEMETADATA)

  SearchDriveMetadataFunction();

 protected:
  virtual ~SearchDriveMetadataFunction();

  virtual bool RunImpl() OVERRIDE;

 private:
  // Callback for SearchMetadata();
  void OnSearchMetadata(drive::FileError error,
                        scoped_ptr<drive::MetadataSearchResultVector> results);
};

class ClearDriveCacheFunction : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.clearDriveCache",
                             FILEBROWSERPRIVATE_CLEARDRIVECACHE)

  ClearDriveCacheFunction();

 protected:
  virtual ~ClearDriveCacheFunction();

  virtual bool RunImpl() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.getDriveConnectionState method.
class GetDriveConnectionStateFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "fileBrowserPrivate.getDriveConnectionState",
      FILEBROWSERPRIVATE_GETDRIVECONNECTIONSTATE);

  GetDriveConnectionStateFunction();

 protected:
  virtual ~GetDriveConnectionStateFunction();

  virtual bool RunImpl() OVERRIDE;
};

// Implements the chrome.fileBrowserPrivate.requestAccessToken method.
class RequestAccessTokenFunction : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.requestAccessToken",
                             FILEBROWSERPRIVATE_REQUESTACCESSTOKEN)

  RequestAccessTokenFunction();

 protected:
  virtual ~RequestAccessTokenFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

  // Callback with a cached auth token (if available) or a fetched one.
  void OnAccessTokenFetched(google_apis::GDataErrorCode code,
                            const std::string& access_token);
};

// Implements the chrome.fileBrowserPrivate.getShareUrl method.
class GetShareUrlFunction : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getShareUrl",
                             FILEBROWSERPRIVATE_GETSHAREURL)

  GetShareUrlFunction();

 protected:
  virtual ~GetShareUrlFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

  // Callback with an url to the sharing dialog as |share_url|, called by
  // FileSystem::GetShareUrl.
  void OnGetShareUrl(drive::FileError error, const GURL& share_url);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_DRIVE_H_
