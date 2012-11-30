// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_BASE_OPERATIONS_H_
#define CHROME_BROWSER_GOOGLE_APIS_BASE_OPERATIONS_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/google_apis/operation_registry.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

class OAuth2AccessTokenFetcher;

namespace base {
class Value;
}  // namespace base

namespace google_apis {

//======================= AuthenticatedOperationInterface ======================

// An interface for implementing an operation used by DriveServiceInterface.
class AuthenticatedOperationInterface {
 public:
  // Callback to DriveServiceInterface upon for re-authentication.
  typedef base::Callback<void(AuthenticatedOperationInterface* operation)>
      ReAuthenticateCallback;

  virtual ~AuthenticatedOperationInterface() {}

  // Starts the actual operation after obtaining an authentication token
  // |auth_token|. User-Agent header will be set to |custom_user_agent| if
  // the value is not empty.
  virtual void Start(const std::string& auth_token,
                     const std::string& custom_user_agent) = 0;

  // Invoked when the authentication failed with an error code |code|.
  virtual void OnAuthFailed(GDataErrorCode code) = 0;

  // Sets the callback to DriveServiceInterface when the operation restarts due
  // to an authentication failure.
  virtual void SetReAuthenticateCallback(
      const ReAuthenticateCallback& callback) = 0;

  // Gets a weak pointer to this operation object. Since operations may be
  // deleted when it is canceled by user action, for posting asynchronous tasks
  // on the authentication operation object, weak pointers have to be used.
  // TODO(kinaba): crbug.com/134814 use more clean life time management than
  // using weak pointers, while deprecating OperationRegistry.
  virtual base::WeakPtr<AuthenticatedOperationInterface> GetWeakPtr() = 0;
};

//============================ UrlFetchOperationBase ===========================

// Callback type for getting the content from URLFetcher::GetResponseAsString().
typedef base::Callback<void(
    GDataErrorCode error,
    scoped_ptr<std::string> content)> GetContentCallback;

// Base class for operations that are fetching URLs.
class UrlFetchOperationBase : public AuthenticatedOperationInterface,
                              public OperationRegistry::Operation,
                              public net::URLFetcherDelegate {
 public:
  // Overridden from AuthenticatedOperationInterface.
  virtual void Start(const std::string& auth_token,
                     const std::string& custom_user_agent) OVERRIDE;

  // Overridden from AuthenticatedOperationInterface.
  virtual void SetReAuthenticateCallback(
      const ReAuthenticateCallback& callback) OVERRIDE;

  // Overridden from AuthenticatedOperationInterface.
  virtual base::WeakPtr<AuthenticatedOperationInterface> GetWeakPtr() OVERRIDE;

 protected:
  explicit UrlFetchOperationBase(OperationRegistry* registry);
  UrlFetchOperationBase(OperationRegistry* registry,
                        OperationType type,
                        const FilePath& path);
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
  virtual void ProcessURLFetchResults(const net::URLFetcher* source) = 0;

  // Invoked when it needs to notify the status. Chunked operations that
  // constructs a logically single operation from multiple physical operations
  // should notify resume/suspend instead of start/finish.
  virtual void NotifyStartToOperationRegistry();
  virtual void NotifySuccessToOperationRegistry();

  // Invoked by this base class upon an authentication error or cancel by
  // an user operation. Must be implemented by a derived class.
  virtual void RunCallbackOnPrematureFailure(GDataErrorCode code) = 0;

  // Implement OperationRegistry::Operation
  virtual void DoCancel() OVERRIDE;

  // Overridden from URLFetcherDelegate.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // Overridden from AuthenticatedOperationInterface.
  virtual void OnAuthFailed(GDataErrorCode code) OVERRIDE;

  // Invoked when ProcessURLFetchResults() is completed.
  void OnProcessURLFetchResultsComplete(bool result);

  // Returns an appropriate GDataErrorCode based on the HTTP response code and
  // the status of the URLFetcher.
  GDataErrorCode GetErrorCode(const net::URLFetcher* source) const;

  std::string GetResponseHeadersAsString(
      const net::URLFetcher* url_fetcher);

  ReAuthenticateCallback re_authenticate_callback_;
  int re_authenticate_count_;
  bool save_temp_file_;
  FilePath output_file_path_;
  scoped_ptr<net::URLFetcher> url_fetcher_;
  bool started_;

  // WeakPtrFactory bound to the UI thread.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<UrlFetchOperationBase> weak_ptr_factory_;
};

//============================ EntryActionOperation ============================

// Callback type for Delete/Move DocumentServiceInterface calls.
typedef base::Callback<void(GDataErrorCode error)> EntryActionCallback;

// This class performs a simple action over a given entry (document/file).
// It is meant to be used for operations that return no JSON blobs.
class EntryActionOperation : public UrlFetchOperationBase {
 public:
  EntryActionOperation(OperationRegistry* registry,
                       const EntryActionCallback& callback);
  virtual ~EntryActionOperation();

 protected:
  // Overridden from UrlFetchOperationBase.
  virtual void ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;
  virtual void RunCallbackOnPrematureFailure(GDataErrorCode code) OVERRIDE;

 private:
  EntryActionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(EntryActionOperation);
};

//============================== GetDataOperation ==============================

// Callback type for DocumentServiceInterface::GetDocuments.
// Note: feed_data argument should be passed using base::Passed(&feed_data), not
// feed_data.Pass().
typedef base::Callback<void(GDataErrorCode error,
                            scoped_ptr<base::Value> feed_data)> GetDataCallback;

// This class performs the operation for fetching and parsing JSON data content.
class GetDataOperation : public UrlFetchOperationBase {
 public:
  GetDataOperation(OperationRegistry* registry,
                   const GetDataCallback& callback);
  virtual ~GetDataOperation();

  // Parse GData JSON response.
  virtual void ParseResponse(GDataErrorCode fetch_error_code,
                             const std::string& data);

 protected:
  // Overridden from UrlFetchOperationBase.
  virtual void ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;
  virtual void RunCallbackOnPrematureFailure(
      GDataErrorCode fetch_error_code) OVERRIDE;
  void RunCallback(GDataErrorCode fetch_error_code,
                   scoped_ptr<base::Value> value);

 private:
  // Called when ParseJsonOnBlockingPool() is completed.
  void OnDataParsed(google_apis::GDataErrorCode fetch_error_code,
                    scoped_ptr<base::Value> value);

  GetDataCallback callback_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<GetDataOperation> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(GetDataOperation);
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_BASE_OPERATIONS_H_
