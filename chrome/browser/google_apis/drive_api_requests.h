// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_DRIVE_API_REQUESTS_H_
#define CHROME_BROWSER_GOOGLE_APIS_DRIVE_API_REQUESTS_H_

#include <string>

#include "base/callback_forward.h"
#include "chrome/browser/google_apis/base_requests.h"
#include "chrome/browser/google_apis/drive_api_url_generator.h"
#include "chrome/browser/google_apis/drive_common_callbacks.h"

namespace google_apis {

class FileResource;

// Callback used for requests that the server returns FileResource data
// formatted into JSON value.
typedef base::Callback<void(GDataErrorCode error,
                            scoped_ptr<FileResource> entry)>
    FileResourceCallback;

//============================ GetChangelistRequest ==========================

// This class performs the request for fetching changelist.
// The result may contain only first part of the result. The remaining result
// should be able to be fetched by ContinueGetFileListRequest defined below.
class GetChangelistRequest : public GetDataRequest {
 public:
  // |include_deleted| specifies if the response should contain the changes
  // for deleted entries or not.
  // |start_changestamp| specifies the starting point of change list or 0 if
  // all changes are necessary.
  // |max_results| specifies the max of the number of files resource in the
  // response.
  GetChangelistRequest(RequestSender* sender,
                       const DriveApiUrlGenerator& url_generator,
                       bool include_deleted,
                       int64 start_changestamp,
                       int max_results,
                       const GetDataCallback& callback);
  virtual ~GetChangelistRequest();

 protected:
  // Overridden from GetDataRequest.
  virtual GURL GetURL() const OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;
  const bool include_deleted_;
  const int64 start_changestamp_;
  const int max_results_;

  DISALLOW_COPY_AND_ASSIGN(GetChangelistRequest);
};

//============================= GetFilelistRequest ===========================

// This class performs the request for fetching Filelist.
// The result may contain only first part of the result. The remaining result
// should be able to be fetched by ContinueGetFileListRequest defined below.
class GetFilelistRequest : public GetDataRequest {
 public:
  GetFilelistRequest(RequestSender* sender,
                     const DriveApiUrlGenerator& url_generator,
                     const std::string& search_string,
                     int max_results,
                     const GetDataCallback& callback);
  virtual ~GetFilelistRequest();

 protected:
  // Overridden from GetDataRequest.
  virtual GURL GetURL() const OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;
  const std::string search_string_;
  const int max_results_;

  DISALLOW_COPY_AND_ASSIGN(GetFilelistRequest);
};

// This namespace is introduced to avoid class name confliction between
// the requests for Drive API v2 and GData WAPI for transition.
// And, when the migration is done and GData WAPI's code is cleaned up,
// classes inside this namespace should be moved to the google_apis namespace.
// TODO(hidehiko): Move all the requests defined in this file into drive
// namespace.  crbug.com/180808
namespace drive {

//=============================== FilesGetRequest =============================

// This class performs the request for fetching a file.
// This request is mapped to
// https://developers.google.com/drive/v2/reference/files/get
class FilesGetRequest : public GetDataRequest {
 public:
  FilesGetRequest(RequestSender* sender,
                  const DriveApiUrlGenerator& url_generator,
                  const FileResourceCallback& callback);
  virtual ~FilesGetRequest();

  // Required parameter.
  const std::string& file_id() const { return file_id_; }
  void set_file_id(const std::string& file_id) { file_id_ = file_id; }

 protected:
  // Overridden from GetDataRequest.
  virtual GURL GetURL() const OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;
  std::string file_id_;

  DISALLOW_COPY_AND_ASSIGN(FilesGetRequest);
};

//============================== FilesPatchRequest ============================

// This class performs the request for patching file metadata.
// This request is mapped to
// https://developers.google.com/drive/v2/reference/files/patch
class FilesPatchRequest : public GetDataRequest {
 public:
  FilesPatchRequest(RequestSender* sender,
                    const DriveApiUrlGenerator& url_generator,
                    const FileResourceCallback& callback);
  virtual ~FilesPatchRequest();

  // Required parameter.
  const std::string& file_id() const { return file_id_; }
  void set_file_id(const std::string& file_id) { file_id_ = file_id; }

  // Optional parameter.
  bool set_modified_date() const { return set_modified_date_; }
  void set_set_modified_date(bool set_modified_date) {
    set_modified_date_ = set_modified_date;
  }

  bool update_viewed_date() const { return update_viewed_date_; }
  void set_update_viewed_date(bool update_viewed_date) {
    update_viewed_date_ = update_viewed_date;
  }

  // Optional request body.
  // Note: "Files: patch" accepts any "Files resource" data, but this class
  // only supports limited members of it for now. We can extend it upon
  // requirments.
  const std::string& title() const { return title_; }
  void set_title(const std::string& title) { title_ = title; }

  const base::Time& modified_date() const { return modified_date_; }
  void set_modified_date(const base::Time& modified_date) {
    modified_date_ = modified_date;
  }

  const base::Time& last_viewed_by_me_date() const {
    return last_viewed_by_me_date_;
  }
  void set_last_viewed_by_me_date(const base::Time& last_viewed_by_me_date) {
    last_viewed_by_me_date_ = last_viewed_by_me_date;
  }

  const std::vector<std::string>& parents() const { return parents_; }
  void add_parent(const std::string& parent) { parents_.push_back(parent); }

 protected:
  // UrlFetchRequestBase overrides.
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;
  virtual GURL GetURL() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;

  std::string file_id_;
  bool set_modified_date_;
  bool update_viewed_date_;

  std::string title_;
  base::Time modified_date_;
  base::Time last_viewed_by_me_date_;
  std::vector<std::string> parents_;

  DISALLOW_COPY_AND_ASSIGN(FilesPatchRequest);
};

//============================== AboutGetRequest =============================

// This class performs the request for fetching About data.
// This request is mapped to
// https://developers.google.com/drive/v2/reference/about/get
class AboutGetRequest : public GetDataRequest {
 public:
  AboutGetRequest(RequestSender* sender,
                  const DriveApiUrlGenerator& url_generator,
                  const AboutResourceCallback& callback);
  virtual ~AboutGetRequest();

 protected:
  // Overridden from GetDataRequest.
  virtual GURL GetURL() const OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;

  DISALLOW_COPY_AND_ASSIGN(AboutGetRequest);
};

//============================= AppsListRequest ============================

// This class performs the request for fetching AppList.
// This request is mapped to
// https://developers.google.com/drive/v2/reference/apps/list
class AppsListRequest : public GetDataRequest {
 public:
  AppsListRequest(RequestSender* sender,
                  const DriveApiUrlGenerator& url_generator,
                  const AppListCallback& callback);
  virtual ~AppsListRequest();

 protected:
  // Overridden from GetDataRequest.
  virtual GURL GetURL() const OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;

  DISALLOW_COPY_AND_ASSIGN(AppsListRequest);
};

//======================= ContinueGetFileListRequest =========================

// This class performs the request to fetch remaining Filelist result.
class ContinueGetFileListRequest : public GetDataRequest {
 public:
  ContinueGetFileListRequest(RequestSender* sender,
                             const GURL& url,
                             const GetDataCallback& callback);
  virtual ~ContinueGetFileListRequest();

 protected:
  virtual GURL GetURL() const OVERRIDE;

 private:
  const GURL url_;

  DISALLOW_COPY_AND_ASSIGN(ContinueGetFileListRequest);
};

//========================== CreateDirectoryRequest ==========================

// This class performs the request for creating a directory.
class CreateDirectoryRequest : public GetDataRequest {
 public:
  CreateDirectoryRequest(RequestSender* sender,
                         const DriveApiUrlGenerator& url_generator,
                         const std::string& parent_resource_id,
                         const std::string& directory_title,
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
  const std::string directory_title_;

  DISALLOW_COPY_AND_ASSIGN(CreateDirectoryRequest);
};

//=========================== TouchResourceRequest ===========================

// This class performs the request to touch a document/file/directory.
// This uses "files.patch" of Drive API v2 rather than "files.touch". See also:
// https://developers.google.com/drive/v2/reference/files/patch, and
// https://developers.google.com/drive/v2/reference/files/touch
class TouchResourceRequest : public GetDataRequest {
 public:
  // |callback| must not be null.
  TouchResourceRequest(RequestSender* sender,
                       const DriveApiUrlGenerator& url_generator,
                       const std::string& resource_id,
                       const base::Time& modified_date,
                       const base::Time& last_viewed_by_me_date,
                       const FileResourceCallback& callback);
  virtual ~TouchResourceRequest();

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

  DISALLOW_COPY_AND_ASSIGN(TouchResourceRequest);
};

//=========================== CopyResourceRequest ============================

// This class performs the request for copying a resource.
//
// Copies the resource with |resource_id| into a directory with
// |parent_resource_id|. The new resource will be named as |new_title|.
// |parent_resource_id| can be empty. In the case, the copy will be created
// directly under the default root directory (this is the default behavior
// of Drive API v2's copy request).
//
// This request corresponds to "Files: copy" request on Drive API v2. See
// also: https://developers.google.com/drive/v2/reference/files/copy
class CopyResourceRequest : public GetDataRequest {
 public:
  // Upon completion, |callback| will be called. |callback| must not be null.
  CopyResourceRequest(RequestSender* sender,
                      const DriveApiUrlGenerator& url_generator,
                      const std::string& resource_id,
                      const std::string& parent_resource_id,
                      const std::string& new_title,
                      const FileResourceCallback& callback);
  virtual ~CopyResourceRequest();

 protected:
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual GURL GetURL() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;
 private:
  const DriveApiUrlGenerator url_generator_;
  const std::string resource_id_;
  const std::string parent_resource_id_;
  const std::string new_title_;

  DISALLOW_COPY_AND_ASSIGN(CopyResourceRequest);
};

//=========================== MoveResourceRequest ============================

// This class performs the request for moving a resource.
//
// Moves the resource with |resource_id| into a directory with
// |parent_resource_id| and renames it as |new_title|.
// |parent_resource_id| can be empty. In the case, the resource will still be
// in the current directory.
//
// This request uses "Files: patch" request on Drive API v2. See also:
// https://developers.google.com/drive/v2/reference/files/patch
class MoveResourceRequest : public GetDataRequest {
 public:
  // Upon completion, |callback| will be called. |callback| must not be null.
  MoveResourceRequest(RequestSender* sender,
                      const DriveApiUrlGenerator& url_generator,
                      const std::string& resource_id,
                      const std::string& parent_resource_id,
                      const std::string& new_title,
                      const FileResourceCallback& callback);
  virtual ~MoveResourceRequest();

 protected:
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;
  virtual GURL GetURL() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;
 private:
  const DriveApiUrlGenerator url_generator_;
  const std::string resource_id_;
  const std::string parent_resource_id_;
  const std::string new_title_;

  DISALLOW_COPY_AND_ASSIGN(MoveResourceRequest);
};

//=========================== TrashResourceRequest ===========================

// This class performs the request for trashing a resource.
//
// According to the document:
// https://developers.google.com/drive/v2/reference/files/trash
// the file resource will be returned from the server, which is not in the
// response from WAPI server. For the transition, we simply ignore the result,
// because now we do not handle resources in trash.
// Note for the naming: the name "trash" comes from the server's request
// name. In order to be consistent with the server, we chose "trash" here,
// although we are preferring the term "remove" in drive/google_api code.
// TODO(hidehiko): Replace the base class to GetDataRequest.
class TrashResourceRequest : public EntryActionRequest {
 public:
  // |callback| must not be null.
  TrashResourceRequest(RequestSender* sender,
                       const DriveApiUrlGenerator& url_generator,
                       const std::string& resource_id,
                       const EntryActionCallback& callback);
  virtual ~TrashResourceRequest();

 protected:
  // UrlFetchRequestBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;
  const std::string resource_id_;

  DISALLOW_COPY_AND_ASSIGN(TrashResourceRequest);
};

//========================== InsertResourceRequest ===========================

// This class performs the request for inserting a resource to a directory.
// Note that this is the request of "Children: insert" of the Drive API v2.
// https://developers.google.com/drive/v2/reference/children/insert.
class InsertResourceRequest : public EntryActionRequest {
 public:
  // |callback| must not be null.
  InsertResourceRequest(RequestSender* sender,
                        const DriveApiUrlGenerator& url_generator,
                        const std::string& parent_resource_id,
                        const std::string& resource_id,
                        const EntryActionCallback& callback);
  virtual ~InsertResourceRequest();

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

  DISALLOW_COPY_AND_ASSIGN(InsertResourceRequest);
};

//========================== DeleteResourceRequest ===========================

// This class performs the request for removing a resource from a directory.
// Note that we use "delete" for the name of this class, which comes from the
// request name of the Drive API v2, although we prefer "remove" for that
// sense in "drive/google_api"
// Also note that this is the request of "Children: delete" of the Drive API
// v2. https://developers.google.com/drive/v2/reference/children/delete
class DeleteResourceRequest : public EntryActionRequest {
 public:
  // |callback| must not be null.
  DeleteResourceRequest(RequestSender* sender,
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

// This class performs the request for initiating the upload of a new file.
class InitiateUploadNewFileRequest : public InitiateUploadRequestBase {
 public:
  // |parent_resource_id| should be the resource id of the parent directory.
  // |title| should be set.
  // See also the comments of InitiateUploadRequestBase for more details
  // about the other parameters.
  InitiateUploadNewFileRequest(RequestSender* sender,
                               const DriveApiUrlGenerator& url_generator,
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

// This class performs the request for initiating the upload of an existing
// file.
class InitiateUploadExistingFileRequest
    : public InitiateUploadRequestBase {
 public:
  // |upload_url| should be the upload_url() of the file
  //    (resumable-create-media URL)
  // |etag| should be set if it is available to detect the upload confliction.
  // See also the comments of InitiateUploadRequestBase for more details
  // about the other parameters.
  InitiateUploadExistingFileRequest(RequestSender* sender,
                                    const DriveApiUrlGenerator& url_generator,
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

// Performs the request for resuming the upload of a file.
class ResumeUploadRequest : public ResumeUploadRequestBase {
 public:
  // See also ResumeUploadRequestBase's comment for parameters meaning.
  // |callback| must not be null. |progress_callback| may be null.
  ResumeUploadRequest(RequestSender* sender,
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

// Performs the request to fetch the current upload status of a file.
class GetUploadStatusRequest : public GetUploadStatusRequestBase {
 public:
  // See also GetUploadStatusRequestBase's comment for parameters meaning.
  // |callback| must not be null.
  GetUploadStatusRequest(RequestSender* sender,
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

//========================== DownloadFileRequest ==========================

// This class performs the request for downloading of a specified file.
class DownloadFileRequest : public DownloadFileRequestBase {
 public:
  // See also DownloadFileRequestBase's comment for parameters meaning.
  DownloadFileRequest(RequestSender* sender,
                      const DriveApiUrlGenerator& url_generator,
                      const std::string& resource_id,
                      const base::FilePath& output_file_path,
                      const DownloadActionCallback& download_action_callback,
                      const GetContentCallback& get_content_callback,
                      const ProgressCallback& progress_callback);
  virtual ~DownloadFileRequest();

  DISALLOW_COPY_AND_ASSIGN(DownloadFileRequest);
};

}  // namespace drive
}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_DRIVE_API_REQUESTS_H_
