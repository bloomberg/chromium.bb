// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_OPERATIONS_BASE_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_OPERATIONS_BASE_H_

#include <string>
#include <vector>

#include "chrome/browser/chromeos/gdata/gdata_operation_registry.h"
#include "chrome/browser/chromeos/gdata/gdata_params.h"
#include "chrome/common/net/gaia/oauth2_access_token_consumer.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

class Profile;
class OAuth2AccessTokenFetcher;

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
                              public net::URLFetcherDelegate {
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
  virtual net::URLFetcher::RequestType GetRequestType() const;
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
  virtual bool ProcessURLFetchResults(const net::URLFetcher* source) = 0;

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
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // Overridden from GDataOperationInterface.
  virtual void OnAuthFailed(GDataErrorCode code) OVERRIDE;

  // Returns an appropriate GDataErrorCode based on the HTTP response code and
  // the status of the URLFetcher.
  GDataErrorCode GetErrorCode(const net::URLFetcher* source) const;

  std::string GetResponseHeadersAsString(
      const net::URLFetcher* url_fetcher);

  Profile* profile_;
  ReAuthenticateCallback re_authenticate_callback_;
  int re_authenticate_count_;
  bool save_temp_file_;
  FilePath output_file_path_;
  scoped_ptr<net::URLFetcher> url_fetcher_;
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
  virtual bool ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;
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
  virtual base::Value* ParseResponse(const std::string& data);

 protected:
  // Overridden from UrlFetchOperationBase.
  virtual bool ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;
  virtual void RunCallbackOnPrematureFailure(GDataErrorCode code) OVERRIDE;

 private:
  GetDataCallback callback_;
  DISALLOW_COPY_AND_ASSIGN(GetDataOperation);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_OPERATIONS_BASE_H_
