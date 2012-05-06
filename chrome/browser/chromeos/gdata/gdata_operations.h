// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_OPERATIONS_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_OPERATIONS_H_
#pragma once

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/stringprintf.h"
#include "chrome/browser/chromeos/gdata/gdata_operation_registry.h"
#include "chrome/browser/chromeos/gdata/gdata_params.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/net/gaia/oauth2_access_token_consumer.h"
#include "chrome/common/net/gaia/oauth2_access_token_fetcher.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"

namespace gdata {

//================================ AuthOperation ===============================

// OAuth2 authorization token retrieval operation.
class AuthOperation : public GDataOperationRegistry::Operation,
                      public OAuth2AccessTokenConsumer {
 public:
  AuthOperation(GDataOperationRegistry* registry,
                Profile* profile,
                const AuthStatusCallback& callback,
                const std::string& refresh_token);
  virtual ~AuthOperation();
  void Start();

  // Overridden from OAuth2AccessTokenConsumer:
  virtual void OnGetTokenSuccess(const std::string& access_token) OVERRIDE;
  virtual void OnGetTokenFailure(const GoogleServiceAuthError& error) OVERRIDE;

  // Overridden from GDataOpertionRegistry::Operation
  virtual void DoCancel() OVERRIDE;

 private:
  Profile* profile_;
  std::string token_;
  AuthStatusCallback callback_;
  scoped_ptr<OAuth2AccessTokenFetcher> oauth2_access_token_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(AuthOperation);
};

//=========================== GDataOperationInterface ==========================

// An interface for implementing an operation used by DocumentsService.
class GDataOperationInterface {
 public:
  // Callback to DocumentsService upon for re-authentication.
  typedef base::Callback<void(GDataOperationInterface* operation)>
      ReAuthenticateCallback;

  virtual ~GDataOperationInterface() {}

  // Starts the actual operation after obtaining an authentication token
  // |auth_token|.
  virtual void Start(const std::string& auth_token) = 0;

  // Invoked when the authentication failed with an error code |code|.
  virtual void OnAuthFailed(GDataErrorCode code) = 0;

  // Sets the callback to DocumentsService when the operation restarts due to
  // an authentication failure.
  virtual void SetReAuthenticateCallback(
      const ReAuthenticateCallback& callback) = 0;
};

//============================ UrlFetchOperationBase ===========================

// Base class for operations that are fetching URLs.
class UrlFetchOperationBase : public GDataOperationInterface,
                              public GDataOperationRegistry::Operation,
                              public content::URLFetcherDelegate {
 public:
  // Overridden from GDataOperationInterface.
  virtual void Start(const std::string& auth_token) OVERRIDE;

  // Overridden from GDataOperationInterface.
  virtual void SetReAuthenticateCallback(
      const ReAuthenticateCallback& callback) OVERRIDE;

 protected:
  UrlFetchOperationBase(GDataOperationRegistry* registry, Profile* profile);
  UrlFetchOperationBase(GDataOperationRegistry* registry,
                        GDataOperationRegistry::OperationType type,
                        const FilePath& path,
                        Profile* profile);
  virtual ~UrlFetchOperationBase();

  // Gets URL for the request.
  virtual GURL GetURL() const = 0;
  // Returns the request type. A derived class should override this method
  // for a request type other than HTTP GET.
  virtual content::URLFetcher::RequestType GetRequestType() const;
  // Returns the extra HTTP headers for the request. A derived class should
  // override this method to specify any extra headers needed for the request.
  virtual std::vector<std::string> GetExtraRequestHeaders() const;
  // Used by a derived class to add any content data to the request.
  // Returns true if |upload_content_type| and |upload_content| are updated
  // with the content type and data for the request.
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content);

  // Invoked by OnURLFetchComplete when the operation completes without an
  // authentication error. Must be implemented by a derived class.
  virtual bool ProcessURLFetchResults(const content::URLFetcher* source) = 0;

  // Invoked when it needs to notify the status. Chunked operations that
  // constructs a logically single operation from multiple physical operations
  // should notify resume/suspend instead of start/finish.
  virtual void NotifyStartToOperationRegistry();
  virtual void NotifySuccessToOperationRegistry();

  // Invoked by this base class upon an authentication error or cancel by
  // an user operation. Must be implemented by a derived class.
  virtual void RunCallbackOnPrematureFailure(GDataErrorCode code) = 0;

  // Implement GDataOperationRegistry::Operation
  virtual void DoCancel() OVERRIDE;

  // Overridden from URLFetcherDelegate.
  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE;

  // Overridden from GDataOperationInterface.
  virtual void OnAuthFailed(GDataErrorCode code) OVERRIDE;

  std::string GetResponseHeadersAsString(
      const content::URLFetcher* url_fetcher);

  Profile* profile_;
  ReAuthenticateCallback re_authenticate_callback_;
  int re_authenticate_count_;
  bool save_temp_file_;
  FilePath output_file_path_;
  scoped_ptr<content::URLFetcher> url_fetcher_;
  bool started_;
};

//============================ EntryActionOperation ============================

// This class performs a simple action over a given entry (document/file).
// It is meant to be used for operations that return no JSON blobs.
class EntryActionOperation : public UrlFetchOperationBase {
 public:
  EntryActionOperation(GDataOperationRegistry* registry,
                       Profile* profile,
                       const EntryActionCallback& callback,
                       const GURL& document_url);
  virtual ~EntryActionOperation();

 protected:
  // Overridden from UrlFetchOperationBase.
  virtual GURL GetURL() const OVERRIDE;
  virtual bool ProcessURLFetchResults(const content::URLFetcher* source)
      OVERRIDE;
  virtual void RunCallbackOnPrematureFailure(GDataErrorCode code) OVERRIDE;

  const GURL& document_url() const { return document_url_; }

 private:
  EntryActionCallback callback_;
  GURL document_url_;

  DISALLOW_COPY_AND_ASSIGN(EntryActionOperation);
};

//============================== GetDataOperation ==============================

// This class performs the operation for fetching and parsing JSON data content.
class GetDataOperation : public UrlFetchOperationBase {
 public:
  GetDataOperation(GDataOperationRegistry* registry,
                   Profile* profile,
                   const GetDataCallback& callback);
  virtual ~GetDataOperation();

  // Parse GData JSON response.
  static base::Value* ParseResponse(const std::string& data);

 protected:
  // Overridden from UrlFetchOperationBase.
  virtual bool ProcessURLFetchResults(const content::URLFetcher* source)
      OVERRIDE;
  virtual void RunCallbackOnPrematureFailure(GDataErrorCode code) OVERRIDE;

 private:
  GetDataCallback callback_;
  DISALLOW_COPY_AND_ASSIGN(GetDataOperation);
};

//============================ GetDocumentsOperation ===========================

// This class performs the operation for fetching a document list.
class GetDocumentsOperation : public GetDataOperation {
 public:
  GetDocumentsOperation(GDataOperationRegistry* registry,
                        Profile* profile,
                        int start_changestamp,
                        const std::string& search_string,
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

  DISALLOW_COPY_AND_ASSIGN(GetDocumentsOperation);
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
  virtual bool ProcessURLFetchResults(const content::URLFetcher* source)
      OVERRIDE;
  virtual void RunCallbackOnPrematureFailure(GDataErrorCode code) OVERRIDE;

  // Overridden from content::URLFetcherDelegate.
  virtual void OnURLFetchDownloadProgress(const content::URLFetcher* source,
                                          int64 current, int64 total) OVERRIDE;
  virtual bool ShouldSendDownloadData() OVERRIDE;
  virtual void OnURLFetchDownloadData(
      const content::URLFetcher* source,
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
  // Overridden from EntryActionOperation.
  virtual content::URLFetcher::RequestType GetRequestType() const OVERRIDE;
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
  virtual content::URLFetcher::RequestType GetRequestType() const OVERRIDE;

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
  virtual content::URLFetcher::RequestType GetRequestType() const OVERRIDE;

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
  virtual content::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;

  // Overridden from UrlFetchOperationBase.
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  FilePath::StringType new_name_;

  DISALLOW_COPY_AND_ASSIGN(RenameResourceOperation);
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
  // Overridden from EntryActionOperation.
  virtual GURL GetURL() const OVERRIDE;

  // Overridden from UrlFetchOperationBase.
  virtual content::URLFetcher::RequestType GetRequestType() const OVERRIDE;
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
  // Overridden from EntryActionOperation.
  virtual GURL GetURL() const OVERRIDE;

  // Overridden from UrlFetchOperationBase.
  virtual content::URLFetcher::RequestType GetRequestType() const OVERRIDE;
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
  virtual bool ProcessURLFetchResults(const content::URLFetcher* source)
      OVERRIDE;
  virtual void NotifySuccessToOperationRegistry() OVERRIDE;
  virtual void RunCallbackOnPrematureFailure(GDataErrorCode code) OVERRIDE;

  // Overridden from UrlFetchOperationBase.
  virtual content::URLFetcher::RequestType GetRequestType() const OVERRIDE;
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
  virtual bool ProcessURLFetchResults(const content::URLFetcher* source)
      OVERRIDE;
  virtual void NotifyStartToOperationRegistry() OVERRIDE;
  virtual void NotifySuccessToOperationRegistry() OVERRIDE;
  virtual void RunCallbackOnPrematureFailure(GDataErrorCode code) OVERRIDE;

  // Overridden from UrlFetchOperationBase.
  virtual content::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

  // Overridden from content::UrlFetcherDelegate
  virtual void OnURLFetchUploadProgress(const content::URLFetcher* source,
                                        int64 current, int64 total) OVERRIDE;

 private:
  ResumeUploadCallback callback_;
  ResumeUploadParams params_;
  bool last_chunk_completed_;

  DISALLOW_COPY_AND_ASSIGN(ResumeUploadOperation);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_OPERATIONS_H_
