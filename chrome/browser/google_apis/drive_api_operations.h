// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_DRIVE_API_OPERATIONS_H_
#define CHROME_BROWSER_GOOGLE_APIS_DRIVE_API_OPERATIONS_H_

#include <string>

#include "base/callback_forward.h"
#include "chrome/browser/google_apis/base_operations.h"
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
class GetAboutOperation : public GetDataOperation {
 public:
  GetAboutOperation(OperationRegistry* registry,
                    net::URLRequestContextGetter* url_request_context_getter,
                    const DriveApiUrlGenerator& url_generator,
                    const GetAboutResourceCallback& callback);
  virtual ~GetAboutOperation();

 protected:
  // Overridden from GetDataOperation.
  virtual GURL GetURL() const OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;

  DISALLOW_COPY_AND_ASSIGN(GetAboutOperation);
};

//============================= GetApplistOperation ============================

// This class performs the operation for fetching Applist.
class GetApplistOperation : public GetDataOperation {
 public:
  GetApplistOperation(OperationRegistry* registry,
                      net::URLRequestContextGetter* url_request_context_getter,
                      const DriveApiUrlGenerator& url_generator,
                      const GetDataCallback& callback);
  virtual ~GetApplistOperation();

 protected:
  // Overridden from GetDataOperation.
  virtual GURL GetURL() const OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;

  DISALLOW_COPY_AND_ASSIGN(GetApplistOperation);
};

//============================ GetChangelistOperation ==========================

// This class performs the operation for fetching changelist.
class GetChangelistOperation : public GetDataOperation {
 public:
  // |start_changestamp| specifies the starting point of change list or 0 if
  // all changes are necessary.
  // |url| specifies URL for document feed fetching operation. If empty URL is
  // passed, the default URL is used and returns the first page of the result.
  // When non-first page result is requested, |url| should be specified.
  GetChangelistOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const DriveApiUrlGenerator& url_generator,
      const GURL& url,
      int64 start_changestamp,
      const GetDataCallback& callback);
  virtual ~GetChangelistOperation();

 protected:
  // Overridden from GetDataOperation.
  virtual GURL GetURL() const OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;
  GURL url_;
  int64 start_changestamp_;

  DISALLOW_COPY_AND_ASSIGN(GetChangelistOperation);
};

//============================= GetFlielistOperation ===========================

// This class performs the operation for fetching Filelist.
class GetFilelistOperation : public GetDataOperation {
 public:
  GetFilelistOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const DriveApiUrlGenerator& url_generator,
      const GURL& url,
      const std::string& search_string,
      const GetDataCallback& callback);
  virtual ~GetFilelistOperation();

 protected:
  // Overridden from GetDataOperation.
  virtual GURL GetURL() const OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;
  GURL url_;
  std::string search_string_;

  DISALLOW_COPY_AND_ASSIGN(GetFilelistOperation);
};

//=============================== GetFlieOperation =============================

// This class performs the operation for fetching a file.
class GetFileOperation : public GetDataOperation {
 public:
  GetFileOperation(OperationRegistry* registry,
                   net::URLRequestContextGetter* url_request_context_getter,
                   const DriveApiUrlGenerator& url_generator,
                   const std::string& file_id,
                   const FileResourceCallback& callback);
  virtual ~GetFileOperation();

 protected:
  // Overridden from GetDataOperation.
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
// TODO(hidehiko): Get rid of this namespace after the migration.
namespace drive {

//========================== CreateDirectoryOperation ==========================

// This class performs the operation for creating a directory.
class CreateDirectoryOperation : public GetDataOperation {
 public:
  CreateDirectoryOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const DriveApiUrlGenerator& url_generator,
      const std::string& parent_resource_id,
      const std::string& directory_name,
      const FileResourceCallback& callback);
  virtual ~CreateDirectoryOperation();

 protected:
  // Overridden from GetDataOperation.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;
  const std::string parent_resource_id_;
  const std::string directory_name_;

  DISALLOW_COPY_AND_ASSIGN(CreateDirectoryOperation);
};

//=========================== RenameResourceOperation ==========================

// This class performs the operation for renaming a document/file/directory.
class RenameResourceOperation : public EntryActionOperation {
 public:
  // |callback| must not be null.
  RenameResourceOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const DriveApiUrlGenerator& url_generator,
      const std::string& resource_id,
      const std::string& new_name,
      const EntryActionCallback& callback);
  virtual ~RenameResourceOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;
  virtual GURL GetURL() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;

  const std::string resource_id_;
  const std::string new_name_;

  DISALLOW_COPY_AND_ASSIGN(RenameResourceOperation);
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
// TODO(hidehiko): Replace the base class to GetDataOperation.
class TrashResourceOperation : public EntryActionOperation {
 public:
  // |callback| must not be null.
  TrashResourceOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const DriveApiUrlGenerator& url_generator,
      const std::string& resource_id,
      const EntryActionCallback& callback);
  virtual ~TrashResourceOperation();

 protected:
  // UrlFetchOperationBase overrides.
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
class InsertResourceOperation : public EntryActionOperation {
 public:
  // |callback| must not be null.
  InsertResourceOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const DriveApiUrlGenerator& url_generator,
      const std::string& parent_resource_id,
      const std::string& resource_id,
      const EntryActionCallback& callback);
  virtual ~InsertResourceOperation();

 protected:
  // UrlFetchOperationBase overrides.
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

//========================== DeleteResourceOperation ===========================

// This class performs the operation for removing a resource from a directory.
// Note that we use "delete" for the name of this class, which comes from the
// operation name of the Drive API v2, although we prefer "remove" for that
// sense in "drive/google_api"
// Also note that this is the operation of "Children: delete" of the Drive API
// v2. https://developers.google.com/drive/v2/reference/children/delete
class DeleteResourceOperation : public EntryActionOperation {
 public:
  // |callback| must not be null.
  DeleteResourceOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const DriveApiUrlGenerator& url_generator,
      const std::string& parent_resource_id,
      const std::string& resource_id,
      const EntryActionCallback& callback);
  virtual ~DeleteResourceOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;
  const std::string parent_resource_id_;
  const std::string resource_id_;

  DISALLOW_COPY_AND_ASSIGN(DeleteResourceOperation);
};

//======================= InitiateUploadNewFileOperation =======================

// This class performs the operation for initiating the upload of a new file.
class InitiateUploadNewFileOperation : public InitiateUploadOperationBase {
 public:
  // |parent_resource_id| should be the resource id of the parent directory.
  // |title| should be set.
  // See also the comments of InitiateUploadOperationBase for more details
  // about the other parameters.
  InitiateUploadNewFileOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const DriveApiUrlGenerator& url_generator,
      const base::FilePath& drive_file_path,
      const std::string& content_type,
      int64 content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const InitiateUploadCallback& callback);
  virtual ~InitiateUploadNewFileOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  const DriveApiUrlGenerator url_generator_;
  const std::string parent_resource_id_;
  const std::string title_;

  DISALLOW_COPY_AND_ASSIGN(InitiateUploadNewFileOperation);
};

}  // namespace drive
}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_DRIVE_API_OPERATIONS_H_
