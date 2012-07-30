// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_OPERATIONS_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_OPERATIONS_H_

#include <string>
#include <vector>

#include "chrome/browser/chromeos/gdata/operations_base.h"

namespace gdata {

//============================ GetDocumentsOperation ===========================

// This class performs the operation for fetching a document list.
class GetDocumentsOperation : public GetDataOperation {
 public:
  GetDocumentsOperation(GDataOperationRegistry* registry,
                        Profile* profile,
                        int start_changestamp,
                        const std::string& search_string,
                        const std::string& directory_resource_id,
                        const GetDataCallback& callback);
  virtual ~GetDocumentsOperation();

  // Sets |url| for document fetching operation. This URL should be set in use
  // case when additional 'pages' of document lists are being fetched.
  void SetUrl(const GURL& url);

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
  GetDocumentEntryOperation(GDataOperationRegistry* registry,
                            Profile* profile,
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
  GetAccountMetadataOperation(GDataOperationRegistry* registry,
                              Profile* profile,
                              const GetDataCallback& callback);
  virtual ~GetAccountMetadataOperation();

 protected:
  // Overridden from GetDataOperation.
  virtual GURL GetURL() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(GetAccountMetadataOperation);
};

//============================ DownloadFileOperation ===========================

// This class performs the operation for downloading of a given document/file.
class DownloadFileOperation : public UrlFetchOperationBase {
 public:
  DownloadFileOperation(
      GDataOperationRegistry* registry,
      Profile* profile,
      const DownloadActionCallback& download_action_callback,
      const GetDownloadDataCallback& get_download_data_callback,
      const GURL& document_url,
      const FilePath& virtual_path,
      const FilePath& output_file_path);
  virtual ~DownloadFileOperation();

 protected:
  // Overridden from UrlFetchOperationBase.
  virtual GURL GetURL() const OVERRIDE;
  virtual bool ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;
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
  GetDownloadDataCallback get_download_data_callback_;
  GURL document_url_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFileOperation);
};

//=========================== DeleteDocumentOperation ==========================

// This class performs the operation for deleting a document.
class DeleteDocumentOperation : public EntryActionOperation {
 public:
  DeleteDocumentOperation(GDataOperationRegistry* registry,
                          Profile* profile,
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
  CreateDirectoryOperation(GDataOperationRegistry* registry,
                           Profile* profile,
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
  CopyDocumentOperation(GDataOperationRegistry* registry,
                        Profile* profile,
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
  RenameResourceOperation(GDataOperationRegistry* registry,
                          Profile* profile,
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
  AuthorizeAppsOperation(GDataOperationRegistry* registry,
                          Profile* profile,
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
  virtual bool ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;

  // Must override GetDataOperation's ParseResponse because the response is XML
  // not JSON.
  virtual base::Value* ParseResponse(const std::string& data) OVERRIDE;
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
  AddResourceToDirectoryOperation(GDataOperationRegistry* registry,
                                  Profile* profile,
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
  RemoveResourceFromDirectoryOperation(GDataOperationRegistry* registry,
                                       Profile* profile,
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

// This class performs the operation for initiating the upload of a file.
class InitiateUploadOperation : public UrlFetchOperationBase {
 public:
  InitiateUploadOperation(GDataOperationRegistry* registry,
                          Profile* profile,
                          const InitiateUploadCallback& callback,
                          const InitiateUploadParams& params);
  virtual ~InitiateUploadOperation();

 protected:
  // Overridden from UrlFetchOperationBase.
  virtual GURL GetURL() const OVERRIDE;
  virtual bool ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;
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

// This class performs the operation for resuming the upload of a file.
class ResumeUploadOperation : public UrlFetchOperationBase {
 public:
  ResumeUploadOperation(GDataOperationRegistry* registry,
                        Profile* profile,
                        const ResumeUploadCallback& callback,
                        const ResumeUploadParams& params);
  virtual ~ResumeUploadOperation();

 protected:
  // Overridden from UrlFetchOperationBase.
  virtual GURL GetURL() const OVERRIDE;
  virtual bool ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;
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

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_OPERATIONS_H_
