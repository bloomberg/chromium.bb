// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_GDATA_WAPI_OPERATIONS_H_
#define CHROME_BROWSER_GOOGLE_APIS_GDATA_WAPI_OPERATIONS_H_

#include <string>
#include <vector>

#include "chrome/browser/google_apis/base_operations.h"
#include "chrome/browser/google_apis/drive_upload_mode.h"
#include "chrome/browser/google_apis/gdata_wapi_url_generator.h"
#include "net/base/io_buffer.h"

namespace google_apis {

class GDataWapiUrlGenerator;
class DocumentEntry;

//============================ GetDocumentsOperation ===========================

// This class performs the operation for fetching a document list.
class GetDocumentsOperation : public GetDataOperation {
 public:
  // override_url:
  //   If empty, a hard-coded base URL of the WAPI server is used to fetch
  //   the first page of the feed. This parameter is used for fetching 2nd
  //   page and onward.
  //
  // start_changestamp:
  //   This parameter specifies the starting point of a delta feed or 0 if a
  //   full feed is necessary.
  //
  // search_string:
  //   If non-empty, fetches a list of documents that match the search
  //   string.
  //
  // shared_with_me:
  //   If true, fetches a list of documents shared to the user, otherwise
  //   fetches a list of documents owned by the user.
  //
  // directory_resource_id:
  //   If non-empty, fetches a list of documents in a particular directory.
  //
  // callback:
  //   Called once the feed is fetched. Must not be null.
  GetDocumentsOperation(OperationRegistry* registry,
                        const GDataWapiUrlGenerator& url_generator,
                        const GURL& override_url,
                        int start_changestamp,
                        const std::string& search_string,
                        bool shared_with_me,
                        const std::string& directory_resource_id,
                        const GetDataCallback& callback);
  virtual ~GetDocumentsOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual GURL GetURL() const OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  const GURL override_url_;
  const int start_changestamp_;
  const std::string search_string_;
  const bool shared_with_me_;
  const std::string directory_resource_id_;

  DISALLOW_COPY_AND_ASSIGN(GetDocumentsOperation);
};

//========================= GetDocumentEntryOperation ==========================

// This class performs the operation for fetching a single document entry.
class GetDocumentEntryOperation : public GetDataOperation {
 public:
  // |callback| must not be null.
  GetDocumentEntryOperation(OperationRegistry* registry,
                            const GDataWapiUrlGenerator& url_generator,
                            const std::string& resource_id,
                            const GetDataCallback& callback);
  virtual ~GetDocumentEntryOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual GURL GetURL() const OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  // Resource id of the requested entry.
  const std::string resource_id_;

  DISALLOW_COPY_AND_ASSIGN(GetDocumentEntryOperation);
};

//========================= GetAccountMetadataOperation ========================

// This class performs the operation for fetching account metadata.
class GetAccountMetadataOperation : public GetDataOperation {
 public:
  // |callback| must not be null.
  GetAccountMetadataOperation(OperationRegistry* registry,
                              const GDataWapiUrlGenerator& url_generator,
                              const GetDataCallback& callback);
  virtual ~GetAccountMetadataOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual GURL GetURL() const OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  DISALLOW_COPY_AND_ASSIGN(GetAccountMetadataOperation);
};

//============================ DownloadFileOperation ===========================

// Callback type for DownloadDocument/DownloadFile DocumentServiceInterface
// calls.
typedef base::Callback<void(GDataErrorCode error,
                            const FilePath& temp_file)> DownloadActionCallback;

// This class performs the operation for downloading of a given document/file.
class DownloadFileOperation : public UrlFetchOperationBase {
 public:
  // download_action_callback:
  //   This callback is called when the download is complete. Must not be null.
  //
  // get_content_callback:
  //   This callback is called when some part of the content is
  //   read. Used to read the download content progressively. May be null.
  //
  // content_url:
  //   Specifies the target file to download.
  //
  // drive_file_path:
  //   Specifies the drive path of the target file. Shown in UI.
  //   TODO(satorux): Remove the drive file path hack. crbug.com/163296
  //
  // output_file_path:
  //   Specifies the file path to save the downloaded file.
  //
  DownloadFileOperation(
      OperationRegistry* registry,
      const DownloadActionCallback& download_action_callback,
      const GetContentCallback& get_content_callback,
      const GURL& content_url,
      const FilePath& drive_file_path,
      const FilePath& output_file_path);
  virtual ~DownloadFileOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual void ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;
  virtual void RunCallbackOnPrematureFailure(GDataErrorCode code) OVERRIDE;

  // net::URLFetcherDelegate overrides.
  virtual void OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                          int64 current, int64 total) OVERRIDE;
  virtual bool ShouldSendDownloadData() OVERRIDE;
  virtual void OnURLFetchDownloadData(
      const net::URLFetcher* source,
      scoped_ptr<std::string> download_data) OVERRIDE;

 private:
  const DownloadActionCallback download_action_callback_;
  const GetContentCallback get_content_callback_;
  const GURL content_url_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFileOperation);
};

//=========================== DeleteDocumentOperation ==========================

// This class performs the operation for deleting a document.
class DeleteDocumentOperation : public EntryActionOperation {
 public:
  // |callback| must not be null.
  DeleteDocumentOperation(OperationRegistry* registry,
                          const EntryActionCallback& callback,
                          const GURL& edit_url);
  virtual ~DeleteDocumentOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;

 private:
  const GURL edit_url_;

  DISALLOW_COPY_AND_ASSIGN(DeleteDocumentOperation);
};

//========================== CreateDirectoryOperation ==========================

// This class performs the operation for creating a directory.
class CreateDirectoryOperation : public GetDataOperation {
 public:
  // A new directory will be created under a directory specified by
  // |parent_content_url|. If this parameter is empty, a new directory will
  // be created in the root directory.
  // |callback| must not be null.
  CreateDirectoryOperation(OperationRegistry* registry,
                           const GDataWapiUrlGenerator& url_generator,
                           const GetDataCallback& callback,
                           const GURL& parent_content_url,
                           const FilePath::StringType& directory_name);
  virtual ~CreateDirectoryOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  const GURL parent_content_url_;
  const FilePath::StringType directory_name_;

  DISALLOW_COPY_AND_ASSIGN(CreateDirectoryOperation);
};

//============================ CopyDocumentOperation ===========================

// This class performs the operation for making a copy of a hosted document.
// Note that this function cannot be used to copy regular files, as it's not
// supported by WAPI.
class CopyDocumentOperation : public GetDataOperation {
 public:
  // |callback| must not be null.
  CopyDocumentOperation(OperationRegistry* registry,
                        const GDataWapiUrlGenerator& url_generator,
                        const GetDataCallback& callback,
                        const std::string& resource_id,
                        const FilePath::StringType& new_name);
  virtual ~CopyDocumentOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual GURL GetURL() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  const std::string resource_id_;
  const FilePath::StringType new_name_;

  DISALLOW_COPY_AND_ASSIGN(CopyDocumentOperation);
};

//=========================== RenameResourceOperation ==========================

// This class performs the operation for renaming a document/file/directory.
class RenameResourceOperation : public EntryActionOperation {
 public:
  // |callback| must not be null.
  RenameResourceOperation(OperationRegistry* registry,
                          const EntryActionCallback& callback,
                          const GURL& edit_url,
                          const FilePath::StringType& new_name);
  virtual ~RenameResourceOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;
  virtual GURL GetURL() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const GURL edit_url_;
  const FilePath::StringType new_name_;

  DISALLOW_COPY_AND_ASSIGN(RenameResourceOperation);
};

//=========================== AuthorizeAppOperation ==========================

// This class performs the operation for authorizing an application specified
// by |app_id| to access a document specified by |edit_url| for .
class AuthorizeAppOperation : public GetDataOperation {
 public:
  // |callback| must not be null.
  AuthorizeAppOperation(OperationRegistry* registry,
                        const GetDataCallback& callback,
                        const GURL& edit_url,
                        const std::string& app_id);
  virtual ~AuthorizeAppOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;
  virtual GURL GetURL() const OVERRIDE;
  virtual void ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;

  // Must override GetDataOperation's ParseResponse because the response is XML
  // not JSON.
  virtual void ParseResponse(GDataErrorCode fetch_error_code,
                             const std::string& data) OVERRIDE;

 private:
  const std::string app_id_;
  const GURL edit_url_;

  DISALLOW_COPY_AND_ASSIGN(AuthorizeAppOperation);
};

//======================= AddResourceToDirectoryOperation ======================

// This class performs the operation for adding a document/file/directory
// to a directory.
class AddResourceToDirectoryOperation : public EntryActionOperation {
 public:
  // |callback| must not be null.
  AddResourceToDirectoryOperation(OperationRegistry* registry,
                                  const GDataWapiUrlGenerator& url_generator,
                                  const EntryActionCallback& callback,
                                  const GURL& parent_content_url,
                                  const GURL& edit_url);
  virtual ~AddResourceToDirectoryOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  const GURL parent_content_url_;
  const GURL edit_url_;

  DISALLOW_COPY_AND_ASSIGN(AddResourceToDirectoryOperation);
};

//==================== RemoveResourceFromDirectoryOperation ====================

// This class performs the operation for adding a document/file/directory
// from a directory.
class RemoveResourceFromDirectoryOperation : public EntryActionOperation {
 public:
  // |callback| must not be null.
  RemoveResourceFromDirectoryOperation(OperationRegistry* registry,
                                       const EntryActionCallback& callback,
                                       const GURL& parent_content_url,
                                       const std::string& resource_id);
  virtual ~RemoveResourceFromDirectoryOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;

 private:
  const std::string resource_id_;
  const GURL parent_content_url_;

  DISALLOW_COPY_AND_ASSIGN(RemoveResourceFromDirectoryOperation);
};

//=========================== InitiateUploadOperation ==========================

// Struct for passing params needed for DriveServiceInterface::InitiateUpload()
// calls.
//
// When uploading a new file (UPLOAD_NEW_FILE):
// - |title| should be set.
// - |upload_location| should be the upload_url() of the parent directory.
//   (resumable-create-media URL)
//
// When updating an existing file (UPLOAD_EXISTING_FILE):
// - |title| should be empty
// - |upload_location| should be the upload_url() of the existing file.
//   (resumable-edit-media URL)
struct InitiateUploadParams {
  InitiateUploadParams(UploadMode upload_mode,
                       const std::string& title,
                       const std::string& content_type,
                       int64 content_length,
                       const GURL& upload_location,
                       const FilePath& drive_file_path);
  ~InitiateUploadParams();

  const UploadMode upload_mode;
  const std::string title;
  const std::string content_type;
  const int64 content_length;
  const GURL upload_location;
  const FilePath drive_file_path;
};

// Callback type for DocumentServiceInterface::InitiateUpload.
typedef base::Callback<void(GDataErrorCode error,
                            const GURL& upload_url)> InitiateUploadCallback;

// This class performs the operation for initiating the upload of a file.
// |callback| will be called with the obtained upload URL. The URL will be
// used with ResumeUploadOperation to upload the content to the server.
//
// Here's the flow of uploading:
// 1) Get the upload URL with InitiateUploadOperation.
// 2) Upload the first 512KB (see kUploadChunkSize in drive_uploader.cc)
//    of the target file to the upload URL
// 3) If there is more data to upload, go to 2).
//
class InitiateUploadOperation : public UrlFetchOperationBase {
 public:
  // |callback| will be called with the upload URL, where upload data is
  // uploaded to with ResumeUploadOperation.
  // |callback| must not be null.
  InitiateUploadOperation(OperationRegistry* registry,
                          const InitiateUploadCallback& callback,
                          const InitiateUploadParams& params);
  virtual ~InitiateUploadOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual void ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;
  virtual void NotifySuccessToOperationRegistry() OVERRIDE;
  virtual void RunCallbackOnPrematureFailure(GDataErrorCode code) OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const InitiateUploadCallback callback_;
  const InitiateUploadParams params_;
  const GURL initiate_upload_url_;

  DISALLOW_COPY_AND_ASSIGN(InitiateUploadOperation);
};

//============================ ResumeUploadOperation ===========================

// Struct for response to ResumeUpload.
// TODO(satorux): Should return the next upload URL. crbug.com/163555
struct ResumeUploadResponse {
  ResumeUploadResponse();
  ResumeUploadResponse(GDataErrorCode code,
                       int64 start_range_received,
                       int64 end_range_received);
  ~ResumeUploadResponse();

  GDataErrorCode code;
  // The values of "Range" header returned from the server. The values are
  // used to continue uploading more data. These are set to -1 if an upload
  // is complete.
  int64 start_range_received;
  int64 end_range_received;  // inclusive. See below.
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
                     const FilePath& drive_file_path);
  ~ResumeUploadParams();

  const UploadMode upload_mode;  // Mode of the upload.
  // Start of range of contents currently stored in |buf|.
  const int64 start_range;
  // End of range of contents currently stored in |buf|. This is inclusive.
  // For instance, if you are to upload the first 500 bytes of date,
  // |start_range| is 0 and |end_range| is 499.
  const int64 end_range;
  const int64 content_length;  // File content-Length.
  const std::string content_type;   // Content-Type of file.
  // Holds current content to be uploaded.
  const scoped_refptr<net::IOBuffer> buf;
  const GURL upload_location;   // Url of where to upload the file to.
  // Drive file path of the file seen in the UI. Not necessary for
  // resuming an upload, but used for adding an entry to OperationRegistry.
  // TODO(satorux): Remove the drive file path hack. crbug.com/163296
  const FilePath drive_file_path;
};

// Callback type for DocumentServiceInterface::ResumeUpload.
typedef base::Callback<void(
    const ResumeUploadResponse& response,
    scoped_ptr<google_apis::DocumentEntry> new_entry)> ResumeUploadCallback;

// This class performs the operation for resuming the upload of a file.
// More specifically, this operation uploads a chunk of data carried in |buf|
// of ResumeUploadResponse.
class ResumeUploadOperation : public UrlFetchOperationBase {
 public:
  // |callback| will be called on completion of the operation.
  // |callback| must not be null.
  //
  // If there is more data to upload, |code| in ResumeUploadParams is set to
  // HTTP_RESUME_INCOMPLETE, and |new_entry| parameter is NULL.
  //
  // If upload is complete, |code| is set to HTTP_CREATED for a new file, or
  // HTTP_SUCCES for an existing file. |new_entry| contains the document
  // entry of the newly uploaded file.
  ResumeUploadOperation(OperationRegistry* registry,
                        const ResumeUploadCallback& callback,
                        const ResumeUploadParams& params);
  virtual ~ResumeUploadOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual void ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;
  virtual void NotifyStartToOperationRegistry() OVERRIDE;
  virtual void NotifySuccessToOperationRegistry() OVERRIDE;
  virtual void RunCallbackOnPrematureFailure(GDataErrorCode code) OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

  // content::UrlFetcherDelegate overrides.
  virtual void OnURLFetchUploadProgress(const net::URLFetcher* source,
                                        int64 current, int64 total) OVERRIDE;

 private:
  const ResumeUploadCallback callback_;
  const ResumeUploadParams params_;
  bool last_chunk_completed_;

  DISALLOW_COPY_AND_ASSIGN(ResumeUploadOperation);
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_GDATA_WAPI_OPERATIONS_H_
