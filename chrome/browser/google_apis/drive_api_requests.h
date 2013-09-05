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

class ChangeList;
class FileResource;
class FileList;

// Callback used for requests that the server returns FileResource data
// formatted into JSON value.
typedef base::Callback<void(GDataErrorCode error,
                            scoped_ptr<FileResource> entry)>
    FileResourceCallback;

// Callback used for requests that the server returns FileList data
// formatted into JSON value.
typedef base::Callback<void(GDataErrorCode error,
                            scoped_ptr<FileList> entry)> FileListCallback;

// Callback used for requests that the server returns ChangeList data
// formatted into JSON value.
typedef base::Callback<void(GDataErrorCode error,
                            scoped_ptr<ChangeList> entry)> ChangeListCallback;

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

//============================ FilesInsertRequest =============================

// This class performs the request for creating a resource.
// This request is mapped to
// https://developers.google.com/drive/v2/reference/files/insert
// See also https://developers.google.com/drive/manage-uploads and
// https://developers.google.com/drive/folder
class FilesInsertRequest : public GetDataRequest {
 public:
  FilesInsertRequest(RequestSender* sender,
                     const DriveApiUrlGenerator& url_generator,
                     const FileResourceCallback& callback);
  virtual ~FilesInsertRequest();

  // Optional request body.
  const std::string& mime_type() const { return mime_type_; }
  void set_mime_type(const std::string& mime_type) {
    mime_type_ = mime_type;
  }

  const std::vector<std::string>& parents() const { return parents_; }
  void add_parent(const std::string& parent) { parents_.push_back(parent); }

  const std::string& title() const { return title_; }
  void set_title(const std::string& title) { title_ = title; }

 protected:
  // Overridden from GetDataRequest.
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual GURL GetURL() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;

  std::string mime_type_;
  std::vector<std::string> parents_;
  std::string title_;

  DISALLOW_COPY_AND_ASSIGN(FilesInsertRequest);
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

//============================= FilesCopyRequest ==============================

// This class performs the request for copying a resource.
// This request is mapped to
// https://developers.google.com/drive/v2/reference/files/copy
class FilesCopyRequest : public GetDataRequest {
 public:
  // Upon completion, |callback| will be called. |callback| must not be null.
  FilesCopyRequest(RequestSender* sender,
                   const DriveApiUrlGenerator& url_generator,
                   const FileResourceCallback& callback);
  virtual ~FilesCopyRequest();

  // Required parameter.
  const std::string& file_id() const { return file_id_; }
  void set_file_id(const std::string& file_id) { file_id_ = file_id; }

  // Optional request body.
  const std::vector<std::string>& parents() const { return parents_; }
  void add_parent(const std::string& parent) { parents_.push_back(parent); }

  const std::string& title() const { return title_; }
  void set_title(const std::string& title) { title_ = title; }

 protected:
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual GURL GetURL() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;

  std::string file_id_;
  std::vector<std::string> parents_;
  std::string title_;

  DISALLOW_COPY_AND_ASSIGN(FilesCopyRequest);
};

//============================= FilesListRequest =============================

// This class performs the request for fetching FileList.
// The result may contain only first part of the result. The remaining result
// should be able to be fetched by ContinueGetFileListRequest defined below,
// or by FilesListRequest with setting page token.
// This request is mapped to
// https://developers.google.com/drive/v2/reference/files/list
class FilesListRequest : public GetDataRequest {
 public:
  FilesListRequest(RequestSender* sender,
                   const DriveApiUrlGenerator& url_generator,
                   const FileListCallback& callback);
  virtual ~FilesListRequest();

  // Optional parameter
  int max_results() const { return max_results_; }
  void set_max_results(int max_results) { max_results_ = max_results; }

  const std::string& page_token() const { return page_token_; }
  void set_page_token(const std::string& page_token) {
    page_token_ = page_token;
  }

  const std::string& q() const { return q_; }
  void set_q(const std::string& q) { q_ = q; }

 protected:
  // Overridden from GetDataRequest.
  virtual GURL GetURL() const OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;
  int max_results_;
  std::string page_token_;
  std::string q_;

  DISALLOW_COPY_AND_ASSIGN(FilesListRequest);
};

//========================= FilesListNextPageRequest ==========================

// There are two ways to obtain next pages of "Files: list" result (if paged).
// 1) Set pageToken and all params used for the initial request.
// 2) Use URL in the nextLink field in the previous response.
// This class implements 2)'s request.
class FilesListNextPageRequest : public GetDataRequest {
 public:
  FilesListNextPageRequest(RequestSender* sender,
                           const FileListCallback& callback);
  virtual ~FilesListNextPageRequest();

  const GURL& next_link() const { return next_link_; }
  void set_next_link(const GURL& next_link) { next_link_ = next_link; }

 protected:
  // Overridden from GetDataRequest.
  virtual GURL GetURL() const OVERRIDE;

 private:
  GURL next_link_;

  DISALLOW_COPY_AND_ASSIGN(FilesListNextPageRequest);
};

//============================= FilesTrashRequest =============================

// This class performs the request for trashing a resource.
// This request is mapped to
// https://developers.google.com/drive/v2/reference/files/trash
class FilesTrashRequest : public GetDataRequest {
 public:
  FilesTrashRequest(RequestSender* sender,
                    const DriveApiUrlGenerator& url_generator,
                    const FileResourceCallback& callback);
  virtual ~FilesTrashRequest();

  // Required parameter.
  const std::string& file_id() const { return file_id_; }
  void set_file_id(const std::string& file_id) { file_id_ = file_id; }

 protected:
  // UrlFetchRequestBase overrides.
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual GURL GetURL() const OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;
  std::string file_id_;

  DISALLOW_COPY_AND_ASSIGN(FilesTrashRequest);
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

//============================ ChangesListRequest ============================

// This class performs the request for fetching ChangeList.
// The result may contain only first part of the result. The remaining result
// should be able to be fetched by ContinueGetFileListRequest defined below.
// or by ChangesListRequest with setting page token.
// This request is mapped to
// https://developers.google.com/drive/v2/reference/changes/list
class ChangesListRequest : public GetDataRequest {
 public:
  ChangesListRequest(RequestSender* sender,
                     const DriveApiUrlGenerator& url_generator,
                     const ChangeListCallback& callback);
  virtual ~ChangesListRequest();

  // Optional parameter
  bool include_deleted() const { return include_deleted_; }
  void set_include_deleted(bool include_deleted) {
    include_deleted_ = include_deleted;
  }

  int max_results() const { return max_results_; }
  void set_max_results(int max_results) { max_results_ = max_results; }

  const std::string& page_token() const { return page_token_; }
  void set_page_token(const std::string& page_token) {
    page_token_ = page_token;
  }

  int64 start_change_id() const { return start_change_id_; }
  void set_start_change_id(int64 start_change_id) {
    start_change_id_ = start_change_id;
  }

 protected:
  // Overridden from GetDataRequest.
  virtual GURL GetURL() const OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;
  bool include_deleted_;
  int max_results_;
  std::string page_token_;
  int64 start_change_id_;

  DISALLOW_COPY_AND_ASSIGN(ChangesListRequest);
};

//======================== ChangesListNextPageRequest =========================

// There are two ways to obtain next pages of "Changes: list" result (if paged).
// 1) Set pageToken and all params used for the initial request.
// 2) Use URL in the nextLink field in the previous response.
// This class implements 2)'s request.
class ChangesListNextPageRequest : public GetDataRequest {
 public:
  ChangesListNextPageRequest(RequestSender* sender,
                             const ChangeListCallback& callback);
  virtual ~ChangesListNextPageRequest();

  const GURL& next_link() const { return next_link_; }
  void set_next_link(const GURL& next_link) { next_link_ = next_link; }

 protected:
  // Overridden from GetDataRequest.
  virtual GURL GetURL() const OVERRIDE;

 private:
  GURL next_link_;

  DISALLOW_COPY_AND_ASSIGN(ChangesListNextPageRequest);
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

//========================== ChildrenInsertRequest ============================

// This class performs the request for inserting a resource to a directory.
// This request is mapped to
// https://developers.google.com/drive/v2/reference/children/insert
class ChildrenInsertRequest : public EntryActionRequest {
 public:
  ChildrenInsertRequest(RequestSender* sender,
                        const DriveApiUrlGenerator& url_generator,
                        const EntryActionCallback& callback);
  virtual ~ChildrenInsertRequest();

  // Required parameter.
  const std::string& folder_id() const { return folder_id_; }
  void set_folder_id(const std::string& folder_id) {
    folder_id_ = folder_id;
  }

  // Required body.
  const std::string& id() const { return id_; }
  void set_id(const std::string& id) { id_ = id; }

 protected:
  // UrlFetchRequestBase overrides.
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual GURL GetURL() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;
  std::string folder_id_;
  std::string id_;

  DISALLOW_COPY_AND_ASSIGN(ChildrenInsertRequest);
};

//========================== ChildrenDeleteRequest ============================

// This class performs the request for removing a resource from a directory.
// This request is mapped to
// https://developers.google.com/drive/v2/reference/children/delete
class ChildrenDeleteRequest : public EntryActionRequest {
 public:
  // |callback| must not be null.
  ChildrenDeleteRequest(RequestSender* sender,
                        const DriveApiUrlGenerator& url_generator,
                        const EntryActionCallback& callback);
  virtual ~ChildrenDeleteRequest();

  // Required parameter.
  const std::string& child_id() const { return child_id_; }
  void set_child_id(const std::string& child_id) {
    child_id_ = child_id;
  }

  const std::string& folder_id() const { return folder_id_; }
  void set_folder_id(const std::string& folder_id) {
    folder_id_ = folder_id;
  }

 protected:
  // UrlFetchRequestBase overrides.
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual GURL GetURL() const OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;
  std::string child_id_;
  std::string folder_id_;

  DISALLOW_COPY_AND_ASSIGN(ChildrenDeleteRequest);
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
class InitiateUploadExistingFileRequest : public InitiateUploadRequestBase {
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
