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

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace google_apis {

class GDataWapiUrlGenerator;
class ResourceEntry;

//============================ GetResourceListOperation ========================

// This class performs the operation for fetching a resource list.
class GetResourceListOperation : public GetDataOperation {
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
  //   If non-empty, fetches a list of resources that match the search
  //   string.
  //
  // shared_with_me:
  //   If true, fetches a list of resources shared to the user, otherwise
  //   fetches a list of resources owned by the user.
  //
  // directory_resource_id:
  //   If non-empty, fetches a list of resources in a particular directory.
  //
  // callback:
  //   Called once the feed is fetched. Must not be null.
  GetResourceListOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const GDataWapiUrlGenerator& url_generator,
      const GURL& override_url,
      int start_changestamp,
      const std::string& search_string,
      bool shared_with_me,
      const std::string& directory_resource_id,
      const GetDataCallback& callback);
  virtual ~GetResourceListOperation();

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

  DISALLOW_COPY_AND_ASSIGN(GetResourceListOperation);
};

//========================= GetResourceEntryOperation ==========================

// This class performs the operation for fetching a single resource entry.
class GetResourceEntryOperation : public GetDataOperation {
 public:
  // |callback| must not be null.
  GetResourceEntryOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const GDataWapiUrlGenerator& url_generator,
      const std::string& resource_id,
      const GetDataCallback& callback);
  virtual ~GetResourceEntryOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual GURL GetURL() const OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  // Resource id of the requested entry.
  const std::string resource_id_;

  DISALLOW_COPY_AND_ASSIGN(GetResourceEntryOperation);
};

//========================= GetAccountMetadataOperation ========================

// This class performs the operation for fetching account metadata.
class GetAccountMetadataOperation : public GetDataOperation {
 public:
  // |callback| must not be null.
  GetAccountMetadataOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
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

//=========================== DeleteResourceOperation ==========================

// This class performs the operation for deleting a resource.
//
// In WAPI, "gd:deleted" means that the resource was put in the trash, and
// "docs:removed" means its permanently gone. Since what the class does is to
// put the resource into trash, we have chosen "Delete" in the name, even though
// we are preferring the term "Remove" in drive/google_api code.
class DeleteResourceOperation : public EntryActionOperation {
 public:
  // |callback| must not be null.
  DeleteResourceOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const GDataWapiUrlGenerator& url_generator,
      const EntryActionCallback& callback,
      const std::string& resource_id,
      const std::string& etag);
  virtual ~DeleteResourceOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  const std::string resource_id_;
  const std::string etag_;

  DISALLOW_COPY_AND_ASSIGN(DeleteResourceOperation);
};

//========================== CreateDirectoryOperation ==========================

// This class performs the operation for creating a directory.
class CreateDirectoryOperation : public GetDataOperation {
 public:
  // A new directory will be created under a directory specified by
  // |parent_resource_id|. If this parameter is empty, a new directory will
  // be created in the root directory.
  // |callback| must not be null.
  CreateDirectoryOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const GDataWapiUrlGenerator& url_generator,
      const GetDataCallback& callback,
      const std::string& parent_resource_id,
      const std::string& directory_name);
  virtual ~CreateDirectoryOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  const std::string parent_resource_id_;
  const std::string directory_name_;

  DISALLOW_COPY_AND_ASSIGN(CreateDirectoryOperation);
};

//============================ CopyHostedDocumentOperation =====================

// This class performs the operation for making a copy of a hosted document.
// Note that this function cannot be used to copy regular files, as it's not
// supported by WAPI.
class CopyHostedDocumentOperation : public GetDataOperation {
 public:
  // |callback| must not be null.
  CopyHostedDocumentOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const GDataWapiUrlGenerator& url_generator,
      const GetDataCallback& callback,
      const std::string& resource_id,
      const std::string& new_name);
  virtual ~CopyHostedDocumentOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual GURL GetURL() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  const std::string resource_id_;
  const std::string new_name_;

  DISALLOW_COPY_AND_ASSIGN(CopyHostedDocumentOperation);
};

//=========================== RenameResourceOperation ==========================

// This class performs the operation for renaming a document/file/directory.
class RenameResourceOperation : public EntryActionOperation {
 public:
  // |callback| must not be null.
  RenameResourceOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const GDataWapiUrlGenerator& url_generator,
      const EntryActionCallback& callback,
      const std::string& resource_id,
      const std::string& new_name);
  virtual ~RenameResourceOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;
  virtual GURL GetURL() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  const std::string resource_id_;
  const std::string new_name_;

  DISALLOW_COPY_AND_ASSIGN(RenameResourceOperation);
};

//=========================== AuthorizeAppOperation ==========================

// This class performs the operation for authorizing an application specified
// by |app_id| to access a document specified by |edit_url| for .
class AuthorizeAppOperation : public GetDataOperation {
 public:
  // |callback| must not be null.
  AuthorizeAppOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
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
  AddResourceToDirectoryOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const GDataWapiUrlGenerator& url_generator,
      const EntryActionCallback& callback,
      const std::string& parent_resource_id,
      const std::string& resource_id);
  virtual ~AddResourceToDirectoryOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  const std::string parent_resource_id_;
  const std::string resource_id_;

  DISALLOW_COPY_AND_ASSIGN(AddResourceToDirectoryOperation);
};

//==================== RemoveResourceFromDirectoryOperation ====================

// This class performs the operation for removing a document/file/directory
// from a directory.
class RemoveResourceFromDirectoryOperation : public EntryActionOperation {
 public:
  // |callback| must not be null.
  RemoveResourceFromDirectoryOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const GDataWapiUrlGenerator& url_generator,
      const EntryActionCallback& callback,
      const std::string& parent_resource_id,
      const std::string& resource_id);
  virtual ~RemoveResourceFromDirectoryOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  const std::string resource_id_;
  const std::string parent_resource_id_;

  DISALLOW_COPY_AND_ASSIGN(RemoveResourceFromDirectoryOperation);
};

//======================= InitiateUploadNewFileOperation =======================

// This class performs the operation for initiating the upload of a new file.
class InitiateUploadNewFileOperation : public InitiateUploadOperationBase {
 public:
  // |title| should be set.
  // |parent_upload_url| should be the upload_url() of the parent directory.
  //   (resumable-create-media URL)
  // See also the comments of InitiateUploadOperationBase for more details
  // about the other parameters.
  InitiateUploadNewFileOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const GDataWapiUrlGenerator& url_generator,
      const InitiateUploadCallback& callback,
      const base::FilePath& drive_file_path,
      const std::string& content_type,
      int64 content_length,
      const std::string& parent_resource_id,
      const std::string& title);
  virtual ~InitiateUploadNewFileOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  const std::string parent_resource_id_;
  const std::string title_;

  DISALLOW_COPY_AND_ASSIGN(InitiateUploadNewFileOperation);
};

//==================== InitiateUploadExistingFileOperation =====================

// This class performs the operation for initiating the upload of an existing
// file.
class InitiateUploadExistingFileOperation
    : public InitiateUploadOperationBase {
 public:
  // |upload_url| should be the upload_url() of the file
  //    (resumable-create-media URL)
  // |etag| should be set if it is available to detect the upload confliction.
  // See also the comments of InitiateUploadOperationBase for more details
  // about the other parameters.
  InitiateUploadExistingFileOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const GDataWapiUrlGenerator& url_generator,
      const InitiateUploadCallback& callback,
      const base::FilePath& drive_file_path,
      const std::string& content_type,
      int64 content_length,
      const std::string& resource_id,
      const std::string& etag);
  virtual ~InitiateUploadExistingFileOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  const std::string resource_id_;
  const std::string etag_;

  DISALLOW_COPY_AND_ASSIGN(InitiateUploadExistingFileOperation);
};

//============================= UploadRangeCallback ============================

// Callback type for DocumentServiceInterface::ResumeUpload and
// DocumentServiceInterface::GetUploadStatus.
// TODO(hidehiko): The dependency to ResourceEntry in GData WAPI looks
// twisted dependency. Should be cleaned up.
typedef base::Callback<void(
    const UploadRangeResponse& response,
    scoped_ptr<ResourceEntry> new_entry)> UploadRangeCallback;

//============================ ResumeUploadOperation ===========================

// Struct for passing params needed for DriveServiceInterface::ResumeUpload()
// calls.
struct ResumeUploadParams {
  ResumeUploadParams(UploadMode upload_mode,
                     int64 start_position,
                     int64 end_position,
                     int64 content_length,
                     const std::string& content_type,
                     scoped_refptr<net::IOBuffer> buf,
                     const GURL& upload_location,
                     const base::FilePath& drive_file_path);
  ~ResumeUploadParams();

  const UploadMode upload_mode;  // Mode of the upload.
  // Start of range of contents currently stored in |buf|.
  const int64 start_position;
  // End of range of contents currently stored in |buf|. This is exclusive.
  // For instance, if you are to upload the first 500 bytes of data,
  // |start_position| is 0 and |end_position| is 500.
  const int64 end_position;
  const int64 content_length;  // File content-Length.
  const std::string content_type;   // Content-Type of file.
  // Holds current content to be uploaded.
  const scoped_refptr<net::IOBuffer> buf;
  const GURL upload_location;   // Url of where to upload the file to.
  // Drive file path of the file seen in the UI. Not necessary for
  // resuming an upload, but used for adding an entry to OperationRegistry.
  // TODO(satorux): Remove the drive file path hack. crbug.com/163296
  const base::FilePath drive_file_path;
};

// This class performs the operation for resuming the upload of a file.
// More specifically, this operation uploads a chunk of data carried in |buf|
// of ResumeUploadResponse.
class ResumeUploadOperation : public UploadRangeOperationBase {
 public:
  // |callback| must not be null. See also UploadRangeOperationBase's
  // constructor for more details.
  ResumeUploadOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const UploadRangeCallback& callback,
      const ResumeUploadParams& params);
  virtual ~ResumeUploadOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

  // content::UrlFetcherDelegate overrides.
  virtual void OnURLFetchUploadProgress(const net::URLFetcher* source,
                                        int64 current, int64 total) OVERRIDE;

  // UploadRangeOperationBase overrides.
  virtual void OnRangeOperationComplete(
      const UploadRangeResponse& response,
      scoped_ptr<base::Value> value) OVERRIDE;

 private:
  const UploadRangeCallback callback_;

  // The parameters for the request. See ResumeUploadParams for the details.
  const int64 start_position_;
  const int64 end_position_;
  const int64 content_length_;
  const std::string content_type_;
  const scoped_refptr<net::IOBuffer> buf_;

  DISALLOW_COPY_AND_ASSIGN(ResumeUploadOperation);
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_GDATA_WAPI_OPERATIONS_H_
