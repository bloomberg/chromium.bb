// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_OPERATIONS_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_OPERATIONS_H_

#include <string>
#include <vector>

#include "chrome/browser/chromeos/gdata/drive_upload_mode.h"
#include "chrome/browser/google_apis/operations_base.h"
#include "net/base/io_buffer.h"

namespace gdata {

class GDataEntry;
class DocumentEntry;

//============================ GetDocumentsOperation ===========================

// This class performs the operation for fetching a document list.
class GetDocumentsOperation : public GetDataOperation {
 public:
  // |start_changestamp| specifies the starting point of change list or 0 if
  // all changes are necessary.
  // |url| specifies URL for documents feed fetching operation. If empty URL is
  // passed, the default URL is used and returns the first page of the result.
  // When non-first page result is requested, |url| should be specified.
  GetDocumentsOperation(OperationRegistry* registry,
                        const GURL& url,
                        int start_changestamp,
                        const std::string& search_string,
                        const std::string& directory_resource_id,
                        const GetDataCallback& callback);
  virtual ~GetDocumentsOperation();

 protected:
  // Overridden from GetDataOperation.
  virtual GURL GetURL() const OVERRIDE;

 private:
  GURL override_url_;
  int start_changestamp_;
  std::string search_string_;
  std::string directory_resource_id_;

  DISALLOW_COPY_AND_ASSIGN(GetDocumentsOperation);
};

//========================= GetDocumentEntryOperation ==========================

// This class performs the operation for fetching a single document entry.
class GetDocumentEntryOperation : public GetDataOperation {
 public:
  GetDocumentEntryOperation(OperationRegistry* registry,
                            const std::string& resource_id,
                            const GetDataCallback& callback);
  virtual ~GetDocumentEntryOperation();

 protected:
  // Overridden from GetGdataOperation.
  virtual GURL GetURL() const OVERRIDE;

 private:
  // Resource id of the requested entry.
  std::string resource_id_;

  DISALLOW_COPY_AND_ASSIGN(GetDocumentEntryOperation);
};

//========================= GetAccountMetadataOperation ========================

// This class performs the operation for fetching account metadata.
class GetAccountMetadataOperation : public GetDataOperation {
 public:
  GetAccountMetadataOperation(OperationRegistry* registry,
                              const GetDataCallback& callback);
  virtual ~GetAccountMetadataOperation();

 protected:
  // Overridden from GetDataOperation.
  virtual GURL GetURL() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(GetAccountMetadataOperation);
};

//============================ DownloadFileOperation ===========================

// Callback type for DownloadDocument/DownloadFile DocumentServiceInterface
// calls.
typedef base::Callback<void(GDataErrorCode error,
                            const GURL& content_url,
                            const FilePath& temp_file)> DownloadActionCallback;

// This class performs the operation for downloading of a given document/file.
class DownloadFileOperation : public UrlFetchOperationBase {
 public:
  DownloadFileOperation(
      OperationRegistry* registry,
      const DownloadActionCallback& download_action_callback,
      const GetContentCallback& get_content_callback,
      const GURL& document_url,
      const FilePath& virtual_path,
      const FilePath& output_file_path);
  virtual ~DownloadFileOperation();

 protected:
  // Overridden from UrlFetchOperationBase.
  virtual GURL GetURL() const OVERRIDE;
  virtual void ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;
  virtual void RunCallbackOnPrematureFailure(GDataErrorCode code) OVERRIDE;

  // Overridden from net::URLFetcherDelegate.
  virtual void OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                          int64 current, int64 total) OVERRIDE;
  virtual bool ShouldSendDownloadData() OVERRIDE;
  virtual void OnURLFetchDownloadData(
      const net::URLFetcher* source,
      scoped_ptr<std::string> download_data) OVERRIDE;

 private:
  DownloadActionCallback download_action_callback_;
  GetContentCallback get_content_callback_;
  GURL document_url_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFileOperation);
};

//=========================== DeleteDocumentOperation ==========================

// This class performs the operation for deleting a document.
class DeleteDocumentOperation : public EntryActionOperation {
 public:
  DeleteDocumentOperation(OperationRegistry* registry,
                          const EntryActionCallback& callback,
                          const GURL& document_url);
  virtual ~DeleteDocumentOperation();

 protected:
  // Overridden from UrlFetchOperationBase.
  virtual GURL GetURL() const OVERRIDE;

  // Overridden from EntryActionOperation.
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeleteDocumentOperation);
};

//========================== CreateDirectoryOperation ==========================

// This class performs the operation for creating a directory.
class CreateDirectoryOperation : public GetDataOperation {
 public:
  // Empty |parent_content_url| will create the directory in the root folder.
  CreateDirectoryOperation(OperationRegistry* registry,
                           const GetDataCallback& callback,
                           const GURL& parent_content_url,
                           const FilePath::StringType& directory_name);
  virtual ~CreateDirectoryOperation();

 protected:
  // Overridden from UrlFetchOperationBase.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;

  // Overridden from UrlFetchOperationBase.
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  GURL parent_content_url_;
  FilePath::StringType directory_name_;

  DISALLOW_COPY_AND_ASSIGN(CreateDirectoryOperation);
};

//============================ CopyDocumentOperation ===========================

// This class performs the operation for making a copy of a document.
class CopyDocumentOperation : public GetDataOperation {
 public:
  CopyDocumentOperation(OperationRegistry* registry,
                        const GetDataCallback& callback,
                        const std::string& resource_id,
                        const FilePath::StringType& new_name);
  virtual ~CopyDocumentOperation();

 protected:
  // Overridden from GetDataOperation.
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;

  // Overridden from UrlFetchOperationBase.
  virtual GURL GetURL() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  std::string resource_id_;
  FilePath::StringType new_name_;

  DISALLOW_COPY_AND_ASSIGN(CopyDocumentOperation);
};

//=========================== RenameResourceOperation ==========================

// This class performs the operation for renaming a document/file/directory.
class RenameResourceOperation : public EntryActionOperation {
 public:
  RenameResourceOperation(OperationRegistry* registry,
                          const EntryActionCallback& callback,
                          const GURL& document_url,
                          const FilePath::StringType& new_name);
  virtual ~RenameResourceOperation();

 protected:
  // Overridden from EntryActionOperation.
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;

  // Overridden from UrlFetchOperationBase.
  virtual GURL GetURL() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  FilePath::StringType new_name_;

  DISALLOW_COPY_AND_ASSIGN(RenameResourceOperation);
};

//=========================== AuthorizeAppOperation ==========================

// This class performs the operation for renaming a document/file/directory.
class AuthorizeAppsOperation : public GetDataOperation {
 public:
  AuthorizeAppsOperation(OperationRegistry* registry,
                         const GetDataCallback& callback,
                         const GURL& document_url,
                         const std::string& app_ids);
  virtual ~AuthorizeAppsOperation();

 protected:
  // Overridden from EntryActionOperation.
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;

  // Overridden from UrlFetchOperationBase.
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;

  // Overridden from GetDataOperation.
  virtual GURL GetURL() const OVERRIDE;
  virtual void ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;

  // Must override GetDataOperation's ParseResponse because the response is XML
  // not JSON.
  virtual void ParseResponse(GDataErrorCode fetch_error_code,
                             const std::string& data) OVERRIDE;

 private:
  std::string app_id_;
  GURL document_url_;

  DISALLOW_COPY_AND_ASSIGN(AuthorizeAppsOperation);
};

//======================= AddResourceToDirectoryOperation ======================

// This class performs the operation for adding a document/file/directory
// to a directory.
class AddResourceToDirectoryOperation : public EntryActionOperation {
 public:
  AddResourceToDirectoryOperation(OperationRegistry* registry,
                                  const EntryActionCallback& callback,
                                  const GURL& parent_content_url,
                                  const GURL& document_url);
  virtual ~AddResourceToDirectoryOperation();

 protected:
  // Overridden from UrlFetchOperationBase.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  GURL parent_content_url_;

  DISALLOW_COPY_AND_ASSIGN(AddResourceToDirectoryOperation);
};

//==================== RemoveResourceFromDirectoryOperation ====================

// This class performs the operation for adding a document/file/directory
// from a directory.
class RemoveResourceFromDirectoryOperation : public EntryActionOperation {
 public:
  RemoveResourceFromDirectoryOperation(OperationRegistry* registry,
                                       const EntryActionCallback& callback,
                                       const GURL& parent_content_url,
                                       const GURL& document_url,
                                       const std::string& resource_id);
  virtual ~RemoveResourceFromDirectoryOperation();

 protected:
  // Overridden from UrlFetchOperationBase.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;

 private:
  std::string resource_id_;
  GURL parent_content_url_;

  DISALLOW_COPY_AND_ASSIGN(RemoveResourceFromDirectoryOperation);
};

//=========================== InitiateUploadOperation ==========================

// Struct for passing params needed for DriveServiceInterface::InitiateUpload()
// calls.
//
// When uploading a new file (UPLOAD_NEW_FILE):
// - |title| should be set.
// - |upload_location| should be the upload_url() of the parent directory.
//
// When updating an existing file (UPLOAD_EXISTING_FILE):
// - |title| should be empty
// - |upload_location| should be the upload_url() of the existing file.
struct InitiateUploadParams {
  InitiateUploadParams(UploadMode upload_mode,
                       const std::string& title,
                       const std::string& content_type,
                       int64 content_length,
                       const GURL& upload_location,
                       const FilePath& virtual_path);
  ~InitiateUploadParams();

  UploadMode upload_mode;
  std::string title;
  std::string content_type;
  int64 content_length;
  GURL upload_location;
  const FilePath& virtual_path;
};

// Callback type for DocumentServiceInterface::InitiateUpload.
typedef base::Callback<void(GDataErrorCode error,
                            const GURL& upload_url)> InitiateUploadCallback;

// This class performs the operation for initiating the upload of a file.
class InitiateUploadOperation : public UrlFetchOperationBase {
 public:
  InitiateUploadOperation(OperationRegistry* registry,
                          const InitiateUploadCallback& callback,
                          const InitiateUploadParams& params);
  virtual ~InitiateUploadOperation();

 protected:
  // Overridden from UrlFetchOperationBase.
  virtual GURL GetURL() const OVERRIDE;
  virtual void ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;
  virtual void NotifySuccessToOperationRegistry() OVERRIDE;
  virtual void RunCallbackOnPrematureFailure(GDataErrorCode code) OVERRIDE;

  // Overridden from UrlFetchOperationBase.
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  InitiateUploadCallback callback_;
  InitiateUploadParams params_;
  GURL initiate_upload_url_;

  DISALLOW_COPY_AND_ASSIGN(InitiateUploadOperation);
};

//============================ ResumeUploadOperation ===========================

// Struct for response to ResumeUpload.
struct ResumeUploadResponse {
  ResumeUploadResponse(GDataErrorCode code,
                       int64 start_range_received,
                       int64 end_range_received);
  ~ResumeUploadResponse();

  GDataErrorCode code;
  int64 start_range_received;
  int64 end_range_received;
  FilePath virtual_path;
};

// Struct for passing params needed for DriveServiceInterface::ResumeUpload()
// calls.
struct ResumeUploadParams {
  ResumeUploadParams(UploadMode upload_mode,
                     int64 start_range,
                     int64 end_range,
                     int64 content_length,
                     const std::string& content_type,
                     scoped_refptr<net::IOBuffer> buf,
                     const GURL& upload_location,
                     const FilePath& virtual_path);
  ~ResumeUploadParams();

  UploadMode upload_mode;  // Mode of the upload.
  int64 start_range;  // Start of range of contents currently stored in |buf|.
  int64 end_range;  // End of range of contents currently stored in |buf|.
  int64 content_length;  // File content-Length.
  std::string content_type;   // Content-Type of file.
  scoped_refptr<net::IOBuffer> buf;  // Holds current content to be uploaded.
  GURL upload_location;   // Url of where to upload the file to.
  // Virtual GData path of the file seen in the UI. Not necessary for
  // resuming an upload, but used for adding an entry to OperationRegistry.
  FilePath virtual_path;
};

// Callback type for DocumentServiceInterface::ResumeUpload.
typedef base::Callback<void(
    const ResumeUploadResponse& response,
    scoped_ptr<gdata::DocumentEntry> new_entry)> ResumeUploadCallback;

// This class performs the operation for resuming the upload of a file.
class ResumeUploadOperation : public UrlFetchOperationBase {
 public:
  ResumeUploadOperation(OperationRegistry* registry,
                        const ResumeUploadCallback& callback,
                        const ResumeUploadParams& params);
  virtual ~ResumeUploadOperation();

 protected:
  // Overridden from UrlFetchOperationBase.
  virtual GURL GetURL() const OVERRIDE;
  virtual void ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;
  virtual void NotifyStartToOperationRegistry() OVERRIDE;
  virtual void NotifySuccessToOperationRegistry() OVERRIDE;
  virtual void RunCallbackOnPrematureFailure(GDataErrorCode code) OVERRIDE;

  // Overridden from UrlFetchOperationBase.
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

  // Overridden from content::UrlFetcherDelegate
  virtual void OnURLFetchUploadProgress(const net::URLFetcher* source,
                                        int64 current, int64 total) OVERRIDE;

 private:
  ResumeUploadCallback callback_;
  ResumeUploadParams params_;
  bool last_chunk_completed_;

  DISALLOW_COPY_AND_ASSIGN(ResumeUploadOperation);
};

//========================== GetContactGroupsOperation =========================

// This class fetches a JSON feed containing a user's contact groups.
class GetContactGroupsOperation : public GetDataOperation {
 public:
  GetContactGroupsOperation(OperationRegistry* registry,
                            const GetDataCallback& callback);
  virtual ~GetContactGroupsOperation();

  void set_feed_url_for_testing(const GURL& url) {
    feed_url_for_testing_ = url;
  }

 protected:
  // Overridden from GetDataOperation.
  virtual GURL GetURL() const OVERRIDE;

 private:
  // If non-empty, URL of the feed to fetch.
  GURL feed_url_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(GetContactGroupsOperation);
};

//============================ GetContactsOperation ============================

// This class fetches a JSON feed containing a user's contacts.
class GetContactsOperation : public GetDataOperation {
 public:
  GetContactsOperation(OperationRegistry* registry,
                       const std::string& group_id,
                       const base::Time& min_update_time,
                       const GetDataCallback& callback);
  virtual ~GetContactsOperation();

  void set_feed_url_for_testing(const GURL& url) {
    feed_url_for_testing_ = url;
  }

 protected:
  // Overridden from GetDataOperation.
  virtual GURL GetURL() const OVERRIDE;

 private:
  // If non-empty, URL of the feed to fetch.
  GURL feed_url_for_testing_;

  // If non-empty, contains the ID of the group whose contacts should be
  // returned.  Group IDs generally look like this:
  // http://www.google.com/m8/feeds/groups/user%40gmail.com/base/6
  std::string group_id_;

  // If is_null() is false, contains a minimum last-updated time that will be
  // used to filter contacts.
  base::Time min_update_time_;

  DISALLOW_COPY_AND_ASSIGN(GetContactsOperation);
};

//========================== GetContactPhotoOperation ==========================

// This class fetches a contact's photo.
class GetContactPhotoOperation : public UrlFetchOperationBase {
 public:
  GetContactPhotoOperation(OperationRegistry* registry,
                           const GURL& photo_url,
                           const GetContentCallback& callback);
  virtual ~GetContactPhotoOperation();

 protected:
  // Overridden from UrlFetchOperationBase.
  virtual GURL GetURL() const OVERRIDE;
  virtual void ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;
  virtual void RunCallbackOnPrematureFailure(GDataErrorCode code) OVERRIDE;

 private:
  // Location of the photo to fetch.
  GURL photo_url_;

  // Callback to which the photo data is passed.
  GetContentCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(GetContactPhotoOperation);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_OPERATIONS_H_
