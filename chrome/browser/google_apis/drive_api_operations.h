// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_DRIVE_API_OPERATIONS_H_
#define CHROME_BROWSER_GOOGLE_APIS_DRIVE_API_OPERATIONS_H_

#include <string>

#include "base/callback_forward.h"
#include "chrome/browser/google_apis/base_requests.h"
#include "chrome/browser/google_apis/drive_api_url_generator.h"
#include "chrome/browser/google_apis/drive_service_interface.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace google_apis {

class FileResource;

// Callback used for operations that the server returns FileResource data
// formatted into JSON value.
typedef base::Callback<void(GDataErrorCode error,
                            scoped_ptr<FileResource> entry)>
    FileResourceCallback;


//============================== GetAboutOperation =============================

// This class performs the operation for fetching About data.
class GetAboutOperation : public GetDataRequest {
 public:
  GetAboutOperation(RequestSender* runner,
                    net::URLRequestContextGetter* url_request_context_getter,
                    const DriveApiUrlGenerator& url_generator,
                    const GetAboutResourceCallback& callback);
  virtual ~GetAboutOperation();

 protected:
  // Overridden from GetDataRequest.
  virtual GURL GetURL() const OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;

  DISALLOW_COPY_AND_ASSIGN(GetAboutOperation);
};

//============================= GetApplistOperation ============================

// This class performs the operation for fetching Applist.
class GetApplistOperation : public GetDataRequest {
 public:
  GetApplistOperation(RequestSender* runner,
                      net::URLRequestContextGetter* url_request_context_getter,
                      const DriveApiUrlGenerator& url_generator,
                      const GetDataCallback& callback);
  virtual ~GetApplistOperation();

 protected:
  // Overridden from GetDataRequest.
  virtual GURL GetURL() const OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;

  DISALLOW_COPY_AND_ASSIGN(GetApplistOperation);
};

//============================ GetChangelistOperation ==========================

// This class performs the operation for fetching changelist.
// The result may contain only first part of the result. The remaining result
// should be able to be fetched by ContinueGetFileListOperation defined below.
class GetChangelistOperation : public GetDataRequest {
 public:
  // |include_deleted| specifies if the response should contain the changes
  // for deleted entries or not.
  // |start_changestamp| specifies the starting point of change list or 0 if
  // all changes are necessary.
  // |max_results| specifies the max of the number of files resource in the
  // response.
  GetChangelistOperation(
      RequestSender* runner,
      net::URLRequestContextGetter* url_request_context_getter,
      const DriveApiUrlGenerator& url_generator,
      bool include_deleted,
      int64 start_changestamp,
      int max_results,
      const GetDataCallback& callback);
  virtual ~GetChangelistOperation();

 protected:
  // Overridden from GetDataRequest.
  virtual GURL GetURL() const OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;
  const bool include_deleted_;
  const int64 start_changestamp_;
  const int max_results_;

  DISALLOW_COPY_AND_ASSIGN(GetChangelistOperation);
};

//============================= GetFilelistOperation ===========================

// This class performs the operation for fetching Filelist.
// The result may contain only first part of the result. The remaining result
// should be able to be fetched by ContinueGetFileListOperation defined below.
class GetFilelistOperation : public GetDataRequest {
 public:
  GetFilelistOperation(
      RequestSender* runner,
      net::URLRequestContextGetter* url_request_context_getter,
      const DriveApiUrlGenerator& url_generator,
      const std::string& search_string,
      int max_results,
      const GetDataCallback& callback);
  virtual ~GetFilelistOperation();

 protected:
  // Overridden from GetDataRequest.
  virtual GURL GetURL() const OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;
  const std::string search_string_;
  const int max_results_;

  DISALLOW_COPY_AND_ASSIGN(GetFilelistOperation);
};

//=============================== GetFileOperation =============================

// This class performs the operation for fetching a file.
class GetFileOperation : public GetDataRequest {
 public:
  GetFileOperation(RequestSender* runner,
                   net::URLRequestContextGetter* url_request_context_getter,
                   const DriveApiUrlGenerator& url_generator,
                   const std::string& file_id,
                   const FileResourceCallback& callback);
  virtual ~GetFileOperation();

 protected:
  // Overridden from GetDataRequest.
  virtual GURL GetURL() const OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;
  std::string file_id_;

  DISALLOW_COPY_AND_ASSIGN(GetFileOperation);
};

// This namespace is introduced to avoid class name confliction between
// the operations for Drive API v2 and GData WAPI for transition.
// And, when the migration is done and GData WAPI's code is cleaned up,
// classes inside this namespace should be moved to the google_apis namespace.
// TODO(hidehiko): Move all the operations defined in this file into drive
// namespace.  crbug.com/180808
namespace drive {

//======================= ContinueGetFileListOperation =========================

// This class performs the operation to fetch remaining Filelist result.
class ContinueGetFileListOperation : public GetDataRequest {
 public:
  ContinueGetFileListOperation(
      RequestSender* runner,
      net::URLRequestContextGetter* url_request_context_getter,
      const GURL& url,
      const GetDataCallback& callback);
  virtual ~ContinueGetFileListOperation();

 protected:
  virtual GURL GetURL() const OVERRIDE;

 private:
  const GURL url_;

  DISALLOW_COPY_AND_ASSIGN(ContinueGetFileListOperation);
};

//========================== CreateDirectoryRequest ==========================

// This class performs the operation for creating a directory.
class CreateDirectoryRequest : public GetDataRequest {
 public:
  CreateDirectoryRequest(
      RequestSender* runner,
      net::URLRequestContextGetter* url_request_context_getter,
      const DriveApiUrlGenerator& url_generator,
      const std::string& parent_resource_id,
      const std::string& directory_name,
      const FileResourceCallback& callback);
  virtual ~CreateDirectoryRequest();

 protected:
  // Overridden from GetDataRequest.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;
  const std::string parent_resource_id_;
  const std::string directory_name_;

  DISALLOW_COPY_AND_ASSIGN(CreateDirectoryRequest);
};

//=========================== RenameResourceRequest ==========================

// This class performs the operation for renaming a document/file/directory.
class RenameResourceRequest : public EntryActionRequest {
 public:
  // |callback| must not be null.
  RenameResourceRequest(
      RequestSender* runner,
      net::URLRequestContextGetter* url_request_context_getter,
      const DriveApiUrlGenerator& url_generator,
      const std::string& resource_id,
      const std::string& new_name,
      const EntryActionCallback& callback);
  virtual ~RenameResourceRequest();

 protected:
  // UrlFetchRequestBase overrides.
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;
  virtual GURL GetURL() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;

  const std::string resource_id_;
  const std::string new_name_;

  DISALLOW_COPY_AND_ASSIGN(RenameResourceRequest);
};

//=========================== TouchResourceOperation ===========================

// This class performs the operation to touch a document/file/directory.
// This uses "files.patch" of Drive API v2 rather than "files.touch". See also:
// https://developers.google.com/drive/v2/reference/files/patch, and
// https://developers.google.com/drive/v2/reference/files/touch
class TouchResourceOperation : public GetDataRequest {
 public:
  // |callback| must not be null.
  TouchResourceOperation(
      RequestSender* runner,
      net::URLRequestContextGetter* url_request_context_getter,
      const DriveApiUrlGenerator& url_generator,
      const std::string& resource_id,
      const base::Time& modified_date,
      const base::Time& last_viewed_by_me_date,
      const FileResourceCallback& callback);
  virtual ~TouchResourceOperation();

 protected:
  // UrlFetchRequestBase overrides.
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;
  virtual GURL GetURL() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;

  const std::string resource_id_;
  const base::Time modified_date_;
  const base::Time last_viewed_by_me_date_;

  DISALLOW_COPY_AND_ASSIGN(TouchResourceOperation);
};

//=========================== CopyResourceOperation ============================

// This class performs the operation for copying a resource.
//
// Copies the resource with |resource_id| into a directory with
// |parent_resource_id|. The new resource will be named as |new_name|.
// |parent_resource_id| can be empty. In the case, the copy will be created
// directly under the default root directory (this is the default behavior
// of Drive API v2's copy operation).
//
// This operation corresponds to "Files: copy" operation on Drive API v2. See
// also: https://developers.google.com/drive/v2/reference/files/copy
class CopyResourceOperation : public GetDataRequest {
 public:
  // Upon completion, |callback| will be called. |callback| must not be null.
  CopyResourceOperation(
      RequestSender* runner,
      net::URLRequestContextGetter* url_request_context_getter,
      const DriveApiUrlGenerator& url_generator,
      const std::string& resource_id,
      const std::string& parent_resource_id,
      const std::string& new_name,
      const FileResourceCallback& callback);
  virtual ~CopyResourceOperation();

 protected:
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual GURL GetURL() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;
 private:
  const DriveApiUrlGenerator url_generator_;
  const std::string resource_id_;
  const std::string parent_resource_id_;
  const std::string new_name_;

  DISALLOW_COPY_AND_ASSIGN(CopyResourceOperation);
};

//=========================== TrashResourceOperation ===========================

// This class performs the operation for trashing a resource.
//
// According to the document:
// https://developers.google.com/drive/v2/reference/files/trash
// the file resource will be returned from the server, which is not in the
// response from WAPI server. For the transition, we simply ignore the result,
// because now we do not handle resources in trash.
// Note for the naming: the name "trash" comes from the server's operation
// name. In order to be consistent with the server, we chose "trash" here,
// although we are preferring the term "remove" in drive/google_api code.
// TODO(hidehiko): Replace the base class to GetDataRequest.
class TrashResourceOperation : public EntryActionRequest {
 public:
  // |callback| must not be null.
  TrashResourceOperation(
      RequestSender* runner,
      net::URLRequestContextGetter* url_request_context_getter,
      const DriveApiUrlGenerator& url_generator,
      const std::string& resource_id,
      const EntryActionCallback& callback);
  virtual ~TrashResourceOperation();

 protected:
  // UrlFetchRequestBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;
  const std::string resource_id_;

  DISALLOW_COPY_AND_ASSIGN(TrashResourceOperation);
};

//========================== InsertResourceOperation ===========================

// This class performs the operation for inserting a resource to a directory.
// Note that this is the operation of "Children: insert" of the Drive API v2.
// https://developers.google.com/drive/v2/reference/children/insert.
class InsertResourceOperation : public EntryActionRequest {
 public:
  // |callback| must not be null.
  InsertResourceOperation(
      RequestSender* runner,
      net::URLRequestContextGetter* url_request_context_getter,
      const DriveApiUrlGenerator& url_generator,
      const std::string& parent_resource_id,
      const std::string& resource_id,
      const EntryActionCallback& callback);
  virtual ~InsertResourceOperation();

 protected:
  // UrlFetchRequestBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;
  const std::string parent_resource_id_;
  const std::string resource_id_;

  DISALLOW_COPY_AND_ASSIGN(InsertResourceOperation);
};

//========================== DeleteResourceRequest ===========================

// This class performs the operation for removing a resource from a directory.
// Note that we use "delete" for the name of this class, which comes from the
// operation name of the Drive API v2, although we prefer "remove" for that
// sense in "drive/google_api"
// Also note that this is the operation of "Children: delete" of the Drive API
// v2. https://developers.google.com/drive/v2/reference/children/delete
class DeleteResourceRequest : public EntryActionRequest {
 public:
  // |callback| must not be null.
  DeleteResourceRequest(
      RequestSender* runner,
      net::URLRequestContextGetter* url_request_context_getter,
      const DriveApiUrlGenerator& url_generator,
      const std::string& parent_resource_id,
      const std::string& resource_id,
      const EntryActionCallback& callback);
  virtual ~DeleteResourceRequest();

 protected:
  // UrlFetchRequestBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;
  const std::string parent_resource_id_;
  const std::string resource_id_;

  DISALLOW_COPY_AND_ASSIGN(DeleteResourceRequest);
};

//======================= InitiateUploadNewFileRequest =======================

// This class performs the operation for initiating the upload of a new file.
class InitiateUploadNewFileRequest : public InitiateUploadRequestBase {
 public:
  // |parent_resource_id| should be the resource id of the parent directory.
  // |title| should be set.
  // See also the comments of InitiateUploadRequestBase for more details
  // about the other parameters.
  InitiateUploadNewFileRequest(
      RequestSender* runner,
      net::URLRequestContextGetter* url_request_context_getter,
      const DriveApiUrlGenerator& url_generator,
      const base::FilePath& drive_file_path,
      const std::string& content_type,
      int64 content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const InitiateUploadCallback& callback);
  virtual ~InitiateUploadNewFileRequest();

 protected:
  // UrlFetchRequestBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;
  const std::string parent_resource_id_;
  const std::string title_;

  DISALLOW_COPY_AND_ASSIGN(InitiateUploadNewFileRequest);
};

//==================== InitiateUploadExistingFileRequest =====================

// This class performs the operation for initiating the upload of an existing
// file.
class InitiateUploadExistingFileRequest
    : public InitiateUploadRequestBase {
 public:
  // |upload_url| should be the upload_url() of the file
  //    (resumable-create-media URL)
  // |etag| should be set if it is available to detect the upload confliction.
  // See also the comments of InitiateUploadRequestBase for more details
  // about the other parameters.
  InitiateUploadExistingFileRequest(
      RequestSender* runner,
      net::URLRequestContextGetter* url_request_context_getter,
      const DriveApiUrlGenerator& url_generator,
      const base::FilePath& drive_file_path,
      const std::string& content_type,
      int64 content_length,
      const std::string& resource_id,
      const std::string& etag,
      const InitiateUploadCallback& callback);
  virtual ~InitiateUploadExistingFileRequest();

 protected:
  // UrlFetchRequestBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;
  const std::string resource_id_;
  const std::string etag_;

  DISALLOW_COPY_AND_ASSIGN(InitiateUploadExistingFileRequest);
};

// Callback used for ResumeUpload() and GetUploadStatus().
typedef base::Callback<void(
    const UploadRangeResponse& response,
    scoped_ptr<FileResource> new_resource)> UploadRangeCallback;

//============================ ResumeUploadRequest ===========================

// Performs the operation for resuming the upload of a file.
class ResumeUploadRequest : public ResumeUploadRequestBase {
 public:
  // See also ResumeUploadRequestBase's comment for parameters meaning.
  // |callback| must not be null. |progress_callback| may be null.
  ResumeUploadRequest(
      RequestSender* runner,
      net::URLRequestContextGetter* url_request_context_getter,
      const base::FilePath& drive_file_path,
      const GURL& upload_location,
      int64 start_position,
      int64 end_position,
      int64 content_length,
      const std::string& content_type,
      const base::FilePath& local_file_path,
      const UploadRangeCallback& callback,
      const ProgressCallback& progress_callback);
  virtual ~ResumeUploadRequest();

 protected:
  // UploadRangeRequestBase overrides.
  virtual void OnRangeRequestComplete(
      const UploadRangeResponse& response,
      scoped_ptr<base::Value> value) OVERRIDE;
  // content::UrlFetcherDelegate overrides.
  virtual void OnURLFetchUploadProgress(const net::URLFetcher* source,
                                        int64 current, int64 total) OVERRIDE;

 private:
  const UploadRangeCallback callback_;
  const ProgressCallback progress_callback_;

  DISALLOW_COPY_AND_ASSIGN(ResumeUploadRequest);
};

//========================== GetUploadStatusRequest ==========================

// Performs the operation to request the current upload status of a file.
class GetUploadStatusRequest : public GetUploadStatusRequestBase {
 public:
  // See also GetUploadStatusRequestBase's comment for parameters meaning.
  // |callback| must not be null.
  GetUploadStatusRequest(
      RequestSender* runner,
      net::URLRequestContextGetter* url_request_context_getter,
      const base::FilePath& drive_file_path,
      const GURL& upload_url,
      int64 content_length,
      const UploadRangeCallback& callback);
  virtual ~GetUploadStatusRequest();

 protected:
  // UploadRangeRequestBase overrides.
  virtual void OnRangeRequestComplete(
      const UploadRangeResponse& response,
      scoped_ptr<base::Value> value) OVERRIDE;

 private:
  const UploadRangeCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(GetUploadStatusRequest);
};


}  // namespace drive
}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_DRIVE_API_OPERATIONS_H_
