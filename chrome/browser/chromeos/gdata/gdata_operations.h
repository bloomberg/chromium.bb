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

// Template for optional OAuth2 authorization HTTP header.
const char kAuthorizationHeaderFormat[] = "Authorization: Bearer %s";
// Template for GData API version HTTP header.
const char kGDataVersionHeader[] = "GData-Version: 3.0";
// Maximum number of attempts for re-authentication per operation.
const int kMaxReAuthenticateAttemptsPerOperation = 1;

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

//============================== UrlFetchOperation =============================

// Base class for operations that are fetching URLs.
template <typename T>
class UrlFetchOperation : public GDataOperationInterface,
                          public GDataOperationRegistry::Operation,
                          public content::URLFetcherDelegate {
 public:
  typedef T CompletionCallback;

  UrlFetchOperation(GDataOperationRegistry* registry,
                    Profile* profile,
                    const CompletionCallback& callback)
      : GDataOperationRegistry::Operation(registry),
        profile_(profile),
        // Completion callback runs on the origin thread that creates
        // this operation object.
        callback_(callback),
        // MessageLoopProxy is used to run |callback| on the origin thread.
        relay_proxy_(base::MessageLoopProxy::current()),
        re_authenticate_count_(0),
        save_temp_file_(false) {
  }

  // Overridden from GDataOperationInterface.
  virtual void Start(const std::string& auth_token) OVERRIDE {
    DCHECK(!auth_token.empty());

    GURL url = GetURL();
    DCHECK(!url.is_empty());
    DVLOG(1) << "URL: " << url.spec();

    url_fetcher_.reset(content::URLFetcher::Create(url, GetRequestType(),
                                                   this));
    url_fetcher_->SetRequestContext(profile_->GetRequestContext());
    // Always set flags to neither send nor save cookies.
    url_fetcher_->SetLoadFlags(
        net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES);
    if (save_temp_file_) {
      url_fetcher_->SaveResponseToTemporaryFile(
          content::BrowserThread::GetMessageLoopProxyForThread(
              content::BrowserThread::FILE));
    }

    // Add request headers.
    // Note that SetExtraRequestHeaders clears the current headers and sets it
    // to the passed-in headers, so calling it for each header will result in
    // only the last header being set in request headers.
    url_fetcher_->AddExtraRequestHeader(kGDataVersionHeader);
    url_fetcher_->AddExtraRequestHeader(
          base::StringPrintf(kAuthorizationHeaderFormat, auth_token.data()));
    std::vector<std::string> headers = GetExtraRequestHeaders();
    for (size_t i = 0; i < headers.size(); ++i) {
      url_fetcher_->AddExtraRequestHeader(headers[i]);
      DVLOG(1) << "Extra header: " << headers[i];
    }

    // Set upload data if available.
    std::string upload_content_type;
    std::string upload_content;
    if (GetContentData(&upload_content_type, &upload_content)) {
      url_fetcher_->SetUploadData(upload_content_type, upload_content);
    }

    // Register to operation registry.
    NotifyStart();

    url_fetcher_->Start();
  }

  // Overridden from GDataOperationInterface.
  virtual void SetReAuthenticateCallback(
      const ReAuthenticateCallback& callback) OVERRIDE {
    DCHECK(re_authenticate_callback_.is_null());

    re_authenticate_callback_ = callback;
  }

 protected:
  virtual ~UrlFetchOperation() {}
  // Gets URL for the request.
  virtual GURL GetURL() const = 0;
  // Returns the request type. A derived class should override this method
  // for a request type other than HTTP GET.
  virtual content::URLFetcher::RequestType GetRequestType() const {
    return content::URLFetcher::GET;
  }
  // Returns the extra HTTP headers for the request. A derived class should
  // override this method to specify any extra headers needed for the request.
  virtual std::vector<std::string> GetExtraRequestHeaders() const {
    return std::vector<std::string>();
  }
  // Used by a derived class to add any content data to the request.
  // Returns true if |upload_content_type| and |upload_content| are updated
  // with the content type and data for the request.
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) {
    return false;
  }

  // Invoked by OnURLFetchComplete when the operation completes without an
  // authentication error. Must be implemented by a derived class.
  virtual void ProcessURLFetchResults(const content::URLFetcher* source) = 0;

  // Invoked by this base class upon an authentication error.
  // Must be implemented by a derived class.
  virtual void RunCallbackOnAuthFailed(GDataErrorCode code) = 0;

  // Implement GDataOperationRegistry::Operation
  virtual void DoCancel() OVERRIDE {
    url_fetcher_.reset(NULL);
  }

  // Implement URLFetcherDelegate.
  // TODO(kinaba): http://crosbug.com/27370
  // Current URLFetcherDelegate notifies only the progress of "download"
  // transfers, and no notification for upload progress in POST/PUT.
  // For some GData operations, however, progress of uploading transfer makes
  // more sense. We need to add a way to track upload status.
  virtual void OnURLFetchDownloadProgress(const content::URLFetcher* source,
                                          int64 current, int64 total) OVERRIDE {
    NotifyProgress(current, total);
  }

  // Overridden from URLFetcherDelegate.
  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE {
    GDataErrorCode code =
        static_cast<GDataErrorCode>(source->GetResponseCode());
    DVLOG(1) << "Response headers:\n" << GetResponseHeadersAsString(source);

    if (code == HTTP_UNAUTHORIZED) {
      if (!re_authenticate_callback_.is_null() &&
          ++re_authenticate_count_ <= kMaxReAuthenticateAttemptsPerOperation) {
        re_authenticate_callback_.Run(this);
        return;
      }

      OnAuthFailed(code);
      return;
    }

    // Overridden by each specialization
    ProcessURLFetchResults(source);
    NotifyFinish(GDataOperationRegistry::OPERATION_COMPLETED);
  }

  // Overridden from GDataOperationInterface.
  virtual void OnAuthFailed(GDataErrorCode code) OVERRIDE {
    RunCallbackOnAuthFailed(code);
    NotifyFinish(GDataOperationRegistry::OPERATION_FAILED);
  }

  std::string GetResponseHeadersAsString(
      const content::URLFetcher* url_fetcher) {
    // net::HttpResponseHeaders::raw_headers(), as the name implies, stores
    // all headers in their raw format, i.e each header is null-terminated.
    // So logging raw_headers() only shows the first header, which is probably
    // the status line.  GetNormalizedHeaders, on the other hand, will show all
    // the headers, one per line, which is probably what we want.
    std::string headers;
    // Check that response code indicates response headers are valid (i.e. not
    // malformed) before we retrieve the headers.
    if (url_fetcher->GetResponseCode() ==
        content::URLFetcher::RESPONSE_CODE_INVALID) {
      headers.assign("Response headers are malformed!!");
    } else {
      url_fetcher->GetResponseHeaders()->GetNormalizedHeaders(&headers);
    }
    return headers;
  }

  Profile* profile_;
  CompletionCallback callback_;
  ReAuthenticateCallback re_authenticate_callback_;
  scoped_refptr<base::MessageLoopProxy> relay_proxy_;
  int re_authenticate_count_;
  bool save_temp_file_;
  scoped_ptr<content::URLFetcher> url_fetcher_;
};

//============================ EntryActionOperation ============================

// This class performs a simple action over a given entry (document/file).
// It is meant to be used for operations that return no JSON blobs.
class EntryActionOperation : public UrlFetchOperation<EntryActionCallback> {
 public:
  EntryActionOperation(GDataOperationRegistry* registry,
                       Profile* profile,
                       const EntryActionCallback& callback,
                       const GURL& document_url);

 protected:
  // Overridden from UrlFetchOperation.
  virtual GURL GetURL() const OVERRIDE;
  virtual void ProcessURLFetchResults(const content::URLFetcher* source)
      OVERRIDE;
  virtual void RunCallbackOnAuthFailed(GDataErrorCode code) OVERRIDE;

  const GURL& document_url() const { return document_url_; }

 private:
  GURL document_url_;

  DISALLOW_COPY_AND_ASSIGN(EntryActionOperation);
};

//============================== GetDataOperation ==============================

// This class performs the operation for fetching and parsing JSON data content.
class GetDataOperation : public UrlFetchOperation<GetDataCallback> {
 public:
  GetDataOperation(GDataOperationRegistry* registry,
                   Profile* profile,
                   const GetDataCallback& callback);

 protected:
  // Overridden from UrlFetchOperation.
  virtual void ProcessURLFetchResults(const content::URLFetcher* source)
      OVERRIDE;
  virtual void RunCallbackOnAuthFailed(GDataErrorCode code) OVERRIDE;

  // Parse GData JSON response.
  static base::Value* ParseResponse(const std::string& data);

 private:
  DISALLOW_COPY_AND_ASSIGN(GetDataOperation);
};

//============================ GetDocumentsOperation ===========================

// This class performs the operation for fetching a document list.
class GetDocumentsOperation : public GetDataOperation {
 public:
  GetDocumentsOperation(GDataOperationRegistry* registry,
                        Profile* profile,
                        const GetDataCallback& callback);

  // Sets |url| for document fetching operation. This URL should be set in use
  // case when additional 'pages' of document lists are being fetched.
  void SetUrl(const GURL& url);

 protected:
  // Overridden from GetDataOperation.
  virtual GURL GetURL() const OVERRIDE;

 private:
  GURL override_url_;

  DISALLOW_COPY_AND_ASSIGN(GetDocumentsOperation);
};

//========================= GetAccountMetadataOperation ========================

// This class performs the operation for fetching account metadata.
class GetAccountMetadataOperation : public GetDataOperation {
 public:
  GetAccountMetadataOperation(GDataOperationRegistry* registry,
                              Profile* profile,
                              const GetDataCallback& callback);

 protected:
  // Overridden from GetDataOperation.
  virtual GURL GetURL() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(GetAccountMetadataOperation);
};

//============================ DownloadFileOperation ===========================

// This class performs the operation for downloading of a given document/file.
class DownloadFileOperation : public UrlFetchOperation<DownloadActionCallback> {
 public:
  DownloadFileOperation(GDataOperationRegistry* registry,
                        Profile* profile,
                        const DownloadActionCallback& callback,
                        const GURL& document_url);

 protected:
  // Overridden from UrlFetchOperation.
  virtual GURL GetURL() const OVERRIDE;
  virtual void ProcessURLFetchResults(const content::URLFetcher* source)
      OVERRIDE;
  virtual void RunCallbackOnAuthFailed(GDataErrorCode code) OVERRIDE;

 private:
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

 protected:
  // Overridden from UrlFetchOperation.
  virtual GURL GetURL() const OVERRIDE;
  virtual content::URLFetcher::RequestType GetRequestType() const OVERRIDE;

  // Overridden from UrlFetchOperation.
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
                        const GURL& document_url,
                        const FilePath::StringType& new_name);

 protected:
  // Overridden from GetDataOperation.
  virtual content::URLFetcher::RequestType GetRequestType() const OVERRIDE;

  // Overridden from UrlFetchOperation.
  virtual GURL GetURL() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  GURL document_url_;
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

 protected:
  // Overridden from EntryActionOperation.
  virtual content::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;

  // Overridden from UrlFetchOperation.
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

 protected:
  // Overridden from EntryActionOperation.
  virtual GURL GetURL() const OVERRIDE;

  // Overridden from UrlFetchOperation.
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

 protected:
  // Overridden from EntryActionOperation.
  virtual GURL GetURL() const OVERRIDE;

  // Overridden from UrlFetchOperation.
  virtual content::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;

 private:
  std::string resource_id_;
  GURL parent_content_url_;

  DISALLOW_COPY_AND_ASSIGN(RemoveResourceFromDirectoryOperation);
};

//=========================== InitiateUploadOperation ==========================

// This class performs the operation for initiating the upload of a file.
class InitiateUploadOperation
    : public UrlFetchOperation<InitiateUploadCallback> {
 public:
  InitiateUploadOperation(GDataOperationRegistry* registry,
                          Profile* profile,
                          const InitiateUploadCallback& callback,
                          const InitiateUploadParams& params);

 protected:
  // Overridden from UrlFetchOperation.
  virtual GURL GetURL() const OVERRIDE;
  virtual void ProcessURLFetchResults(const content::URLFetcher* source)
      OVERRIDE;
  virtual void RunCallbackOnAuthFailed(GDataErrorCode code) OVERRIDE;

  // Overridden from UrlFetchOperation.
  virtual content::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  InitiateUploadParams params_;
  GURL initiate_upload_url_;

  DISALLOW_COPY_AND_ASSIGN(InitiateUploadOperation);
};

//============================ ResumeUploadOperation ===========================

// This class performs the operation for resuming the upload of a file.
class ResumeUploadOperation : public UrlFetchOperation<ResumeUploadCallback> {
 public:
  ResumeUploadOperation(GDataOperationRegistry* registry,
                        Profile* profile,
                        const ResumeUploadCallback& callback,
                        const ResumeUploadParams& params);

 protected:
  // Overridden from UrlFetchOperation.
  virtual GURL GetURL() const OVERRIDE;
  virtual void ProcessURLFetchResults(const content::URLFetcher* source)
      OVERRIDE;
  virtual void RunCallbackOnAuthFailed(GDataErrorCode code) OVERRIDE;

  // Overridden from UrlFetchOperation.
  virtual content::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

 private:
  ResumeUploadParams params_;

  DISALLOW_COPY_AND_ASSIGN(ResumeUploadOperation);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_OPERATIONS_H_
