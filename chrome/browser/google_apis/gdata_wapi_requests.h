// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_GDATA_WAPI_REQUESTS_H_
#define CHROME_BROWSER_GOOGLE_APIS_GDATA_WAPI_REQUESTS_H_

#include <string>
#include <vector>

#include "chrome/browser/google_apis/base_requests.h"
#include "chrome/browser/google_apis/drive_common_callbacks.h"
#include "chrome/browser/google_apis/gdata_wapi_url_generator.h"

namespace google_apis {

class AccountMetadata;
class GDataWapiUrlGenerator;
class ResourceEntry;

//============================ GetResourceListRequest ========================

// This class performs the request for fetching a resource list.
class GetResourceListRequest : public GetDataRequest {
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
  // directory_resource_id:
  //   If non-empty, fetches a list of resources in a particular directory.
  //
  // callback:
  //   Called once the feed is fetched. Must not be null.
  GetResourceListRequest(RequestSender* sender,
                         const GDataWapiUrlGenerator& url_generator,
                         const GURL& override_url,
                         int64 start_changestamp,
                         const std::string& search_string,
                         const std::string& directory_resource_id,
                         const GetResourceListCallback& callback);
  virtual ~GetResourceListRequest();

 protected:
  // UrlFetchRequestBase overrides.
  virtual GURL GetURL() const OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  const GURL override_url_;
  const int64 start_changestamp_;
  const std::string search_string_;
  const std::string directory_resource_id_;

  DISALLOW_COPY_AND_ASSIGN(GetResourceListRequest);
};

//============================ SearchByTitleRequest ==========================

// This class performs the request for searching resources by title.
class SearchByTitleRequest : public GetDataRequest {
 public:
  // title: the search query.
  //
  // directory_resource_id: If given (non-empty), the search target is
  //   directly under the directory with the |directory_resource_id|.
  //   If empty, the search target is all the existing resources.
  //
  // callback:
  //   Called once the feed is fetched. Must not be null.
  SearchByTitleRequest(RequestSender* sender,
                       const GDataWapiUrlGenerator& url_generator,
                       const std::string& title,
                       const std::string& directory_resource_id,
                       const GetResourceListCallback& callback);
  virtual ~SearchByTitleRequest();

 protected:
  // UrlFetchRequestBase overrides.
  virtual GURL GetURL() const OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  const std::string title_;
  const std::string directory_resource_id_;

  DISALLOW_COPY_AND_ASSIGN(SearchByTitleRequest);
};

//========================= GetResourceEntryRequest ==========================

// This class performs the request for fetching a single resource entry.
class GetResourceEntryRequest : public GetDataRequest {
 public:
  // |callback| must not be null.
  GetResourceEntryRequest(RequestSender* sender,
                          const GDataWapiUrlGenerator& url_generator,
                          const std::string& resource_id,
                          const GURL& embed_origin,
                          const GetDataCallback& callback);
  virtual ~GetResourceEntryRequest();

 protected:
  // UrlFetchRequestBase overrides.
  virtual GURL GetURL() const OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  // Resource id of the requested entry.
  const std::string resource_id_;
  // Embed origin for an url to the sharing dialog. Can be empty.
  const GURL& embed_origin_;

  DISALLOW_COPY_AND_ASSIGN(GetResourceEntryRequest);
};

//========================= GetAccountMetadataRequest ========================

// Callback used for GetAccountMetadata().
typedef base::Callback<void(GDataErrorCode error,
                            scoped_ptr<AccountMetadata> account_metadata)>
    GetAccountMetadataCallback;

// This class performs the request for fetching account metadata.
class GetAccountMetadataRequest : public GetDataRequest {
 public:
  // If |include_installed_apps| is set to true, the result should include
  // the list of installed third party applications.
  // |callback| must not be null.
  GetAccountMetadataRequest(RequestSender* sender,
                            const GDataWapiUrlGenerator& url_generator,
                            const GetAccountMetadataCallback& callback,
                            bool include_installed_apps);
  virtual ~GetAccountMetadataRequest();

 protected:
  // UrlFetchRequestBase overrides.
  virtual GURL GetURL() const OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  const bool include_installed_apps_;

  DISALLOW_COPY_AND_ASSIGN(GetAccountMetadataRequest);
};

//=========================== DeleteResourceRequest ==========================

// This class performs the request for deleting a resource.
//
// In WAPI, "gd:deleted" means that the resource was put in the trash, and
// "docs:removed" means its permanently gone. Since what the class does is to
// put the resource into trash, we have chosen "Delete" in the name, even though
// we are preferring the term "Remove" in drive/google_api code.
class DeleteResourceRequest : public EntryActionRequest {
 public:
  // |callback| must not be null.
  DeleteResourceRequest(RequestSender* sender,
                        const GDataWapiUrlGenerator& url_generator,
                        const EntryActionCallback& callback,
                        const std::string& resource_id,
                        const std::string& etag);
  virtual ~DeleteResourceRequest();

 protected:
  // UrlFetchRequestBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  const std::string resource_id_;
  const std::string etag_;

  DISALLOW_COPY_AND_ASSIGN(DeleteResourceRequest);
};

//========================== CreateDirectoryRequest ==========================

// This class performs the request for creating a directory.
class CreateDirectoryRequest : public GetDataRequest {
 public:
  // A new directory will be created under a directory specified by
  // |parent_resource_id|. If this parameter is empty, a new directory will
  // be created in the root directory.
  // |callback| must not be null.
  CreateDirectoryRequest(RequestSender* sender,
                         const GDataWapiUrlGenerator& url_generator,
                         const GetDataCallback& callback,
                         const std::string& parent_resource_id,
                         const std::string& directory_title);
  virtual ~CreateDirectoryRequest();

 protected:
  // UrlFetchRequestBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  const std::string parent_resource_id_;
  const std::string directory_title_;

  DISALLOW_COPY_AND_ASSIGN(CreateDirectoryRequest);
};

//============================ CopyHostedDocumentRequest =====================

// This class performs the request for making a copy of a hosted document.
// Note that this function cannot be used to copy regular files, as it's not
// supported by WAPI.
class CopyHostedDocumentRequest : public GetDataRequest {
 public:
  // |callback| must not be null.
  CopyHostedDocumentRequest(RequestSender* sender,
                            const GDataWapiUrlGenerator& url_generator,
                            const GetDataCallback& callback,
                            const std::string& resource_id,
                            const std::string& new_title);
  virtual ~CopyHostedDocumentRequest();

 protected:
  // UrlFetchRequestBase overrides.
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual GURL GetURL() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  const std::string resource_id_;
  const std::string new_title_;

  DISALLOW_COPY_AND_ASSIGN(CopyHostedDocumentRequest);
};

//=========================== RenameResourceRequest ==========================

// This class performs the request for renaming a document/file/directory.
class RenameResourceRequest : public EntryActionRequest {
 public:
  // |callback| must not be null.
  RenameResourceRequest(RequestSender* sender,
                        const GDataWapiUrlGenerator& url_generator,
                        const EntryActionCallback& callback,
                        const std::string& resource_id,
                        const std::string& new_title);
  virtual ~RenameResourceRequest();

 protected:
  // UrlFetchRequestBase overrides.
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;
  virtual GURL GetURL() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  const std::string resource_id_;
  const std::string new_title_;

  DISALLOW_COPY_AND_ASSIGN(RenameResourceRequest);
};

//=========================== AuthorizeAppRequest ==========================

// This class performs the request for authorizing an application specified
// by |app_id| to access a document specified by |resource_id|.
class AuthorizeAppRequest : public GetDataRequest {
 public:
  // |callback| must not be null.
  AuthorizeAppRequest(RequestSender* sender,
                      const GDataWapiUrlGenerator& url_generator,
                      const AuthorizeAppCallback& callback,
                      const std::string& resource_id,
                      const std::string& app_id);
  virtual ~AuthorizeAppRequest();

 protected:
  // UrlFetchRequestBase overrides.
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;
  virtual GURL GetURL() const OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  const std::string resource_id_;
  const std::string app_id_;

  DISALLOW_COPY_AND_ASSIGN(AuthorizeAppRequest);
};

//======================= AddResourceToDirectoryRequest ======================

// This class performs the request for adding a document/file/directory
// to a directory.
class AddResourceToDirectoryRequest : public EntryActionRequest {
 public:
  // |callback| must not be null.
  AddResourceToDirectoryRequest(RequestSender* sender,
                                const GDataWapiUrlGenerator& url_generator,
                                const EntryActionCallback& callback,
                                const std::string& parent_resource_id,
                                const std::string& resource_id);
  virtual ~AddResourceToDirectoryRequest();

 protected:
  // UrlFetchRequestBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  const std::string parent_resource_id_;
  const std::string resource_id_;

  DISALLOW_COPY_AND_ASSIGN(AddResourceToDirectoryRequest);
};

//==================== RemoveResourceFromDirectoryRequest ====================

// This class performs the request for removing a document/file/directory
// from a directory.
class RemoveResourceFromDirectoryRequest : public EntryActionRequest {
 public:
  // |callback| must not be null.
  RemoveResourceFromDirectoryRequest(RequestSender* sender,
                                     const GDataWapiUrlGenerator& url_generator,
                                     const EntryActionCallback& callback,
                                     const std::string& parent_resource_id,
                                     const std::string& resource_id);
  virtual ~RemoveResourceFromDirectoryRequest();

 protected:
  // UrlFetchRequestBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  const std::string resource_id_;
  const std::string parent_resource_id_;

  DISALLOW_COPY_AND_ASSIGN(RemoveResourceFromDirectoryRequest);
};

//======================= InitiateUploadNewFileRequest =======================

// This class performs the request for initiating the upload of a new file.
class InitiateUploadNewFileRequest : public InitiateUploadRequestBase {
 public:
  // |title| should be set.
  // |parent_upload_url| should be the upload_url() of the parent directory.
  //   (resumable-create-media URL)
  // See also the comments of InitiateUploadRequestBase for more details
  // about the other parameters.
  InitiateUploadNewFileRequest(RequestSender* sender,
                               const GDataWapiUrlGenerator& url_generator,
                               const InitiateUploadCallback& callback,
                               const std::string& content_type,
                               int64 content_length,
                               const std::string& parent_resource_id,
                               const std::string& title);
  virtual ~InitiateUploadNewFileRequest();

 protected:
  // UrlFetchRequestBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
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
                                    const GDataWapiUrlGenerator& url_generator,
                                    const InitiateUploadCallback& callback,
                                    const std::string& content_type,
                                    int64 content_length,
                                    const std::string& resource_id,
                                    const std::string& etag);
  virtual ~InitiateUploadExistingFileRequest();

 protected:
  // UrlFetchRequestBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const GDataWapiUrlGenerator url_generator_;
  const std::string resource_id_;
  const std::string etag_;

  DISALLOW_COPY_AND_ASSIGN(InitiateUploadExistingFileRequest);
};

//============================ ResumeUploadRequest ===========================

// Performs the request for resuming the upload of a file.
class ResumeUploadRequest : public ResumeUploadRequestBase {
 public:
  // See also ResumeUploadRequestBase's comment for parameters meaning.
  // |callback| must not be null.
  ResumeUploadRequest(RequestSender* sender,
                      const UploadRangeCallback& callback,
                      const ProgressCallback& progress_callback,
                      const GURL& upload_location,
                      int64 start_position,
                      int64 end_position,
                      int64 content_length,
                      const std::string& content_type,
                      const base::FilePath& local_file_path);
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

// Performs the request to request the current upload status of a file.
class GetUploadStatusRequest : public GetUploadStatusRequestBase {
 public:
  // See also GetUploadStatusRequestBase's comment for parameters meaning.
  // |callback| must not be null.
  GetUploadStatusRequest(RequestSender* sender,
                         const UploadRangeCallback& callback,
                         const GURL& upload_url,
                         int64 content_length);
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
                      const GDataWapiUrlGenerator& url_generator,
                      const DownloadActionCallback& download_action_callback,
                      const GetContentCallback& get_content_callback,
                      const ProgressCallback& progress_callback,
                      const std::string& resource_id,
                      const base::FilePath& output_file_path);
  virtual ~DownloadFileRequest();

  DISALLOW_COPY_AND_ASSIGN(DownloadFileRequest);
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_GDATA_WAPI_REQUESTS_H_
