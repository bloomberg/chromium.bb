// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides base classes used to implement operations for Google APIs.

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

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace google_apis {

//======================= AuthenticatedOperationInterface ======================

// An interface class for implementing an operation which requires OAuth2
// authentication.
class AuthenticatedOperationInterface {
 public:
  // Called when re-authentication is required. See Start() for details.
  typedef base::Callback<void(AuthenticatedOperationInterface* operation)>
      ReAuthenticateCallback;

  virtual ~AuthenticatedOperationInterface() {}

  // Starts the operation with |access_token|. User-Agent header will be set
  // to |custom_user_agent| if the value is not empty.
  //
  // |callback| is called when re-authentication is needed for a certain
  // number of times (see kMaxReAuthenticateAttemptsPerOperation in .cc).
  // The callback should retry by calling Start() again with a new access
  // token, or just call OnAuthFailed() if a retry is not attempted.
  // |callback| must not be null.
  virtual void Start(const std::string& access_token,
                     const std::string& custom_user_agent,
                     const ReAuthenticateCallback& callback) = 0;

  // Invoked when the authentication failed with an error code |code|.
  virtual void OnAuthFailed(GDataErrorCode code) = 0;

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
  // AuthenticatedOperationInterface overrides.
  virtual void Start(const std::string& access_token,
                     const std::string& custom_user_agent,
                     const ReAuthenticateCallback& callback) OVERRIDE;
  virtual base::WeakPtr<AuthenticatedOperationInterface> GetWeakPtr() OVERRIDE;

 protected:
  explicit UrlFetchOperationBase(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter);
  // Use this constructor when you need to implement operations that take a
  // drive file path (ex. for downloading and uploading).
  // |url_request_context_getter| is used to initialize URLFetcher.
  // TODO(satorux): Remove the drive file path hack. crbug.com/163296
  UrlFetchOperationBase(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      OperationType type,
      const FilePath& drive_file_path);
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

  // Invoked when ProcessURLFetchResults() is completed.
  void OnProcessURLFetchResultsComplete(bool result);

  // Returns an appropriate GDataErrorCode based on the HTTP response code and
  // the status of the URLFetcher.
  static GDataErrorCode GetErrorCode(const net::URLFetcher* source);

  // By default, no temporary file will be saved. Derived classes can set
  // this to true in their constructors, if they want to save the downloaded
  // content to a temporary file.
  void set_save_temp_file(bool save_temp_file) {
    save_temp_file_ = save_temp_file;
  }

  // By default, no file will be saved. Derived classes can set an output
  // file path in their constructors, if they want to save the donwloaded
  // content to a file at a specific path.
  void set_output_file_path(const FilePath& output_file_path) {
    output_file_path_ = output_file_path;
  }

 private:
  // OperationRegistry::Operation overrides.
  virtual void DoCancel() OVERRIDE;

  // URLFetcherDelegate overrides.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // AuthenticatedOperationInterface overrides.
  virtual void OnAuthFailed(GDataErrorCode code) OVERRIDE;

  net::URLRequestContextGetter* url_request_context_getter_;
  ReAuthenticateCallback re_authenticate_callback_;
  int re_authenticate_count_;
  scoped_ptr<net::URLFetcher> url_fetcher_;
  bool started_;

  bool save_temp_file_;
  FilePath output_file_path_;

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
  // |url_request_context_getter| is used to initialize URLFetcher.
  // |callback| must not be null.
  EntryActionOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const EntryActionCallback& callback);
  virtual ~EntryActionOperation();

 protected:
  // Overridden from UrlFetchOperationBase.
  virtual void ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;
  virtual void RunCallbackOnPrematureFailure(GDataErrorCode code) OVERRIDE;

 private:
  const EntryActionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(EntryActionOperation);
};

//============================== GetDataOperation ==============================

// Callback type for DocumentServiceInterface::GetResourceList.
// Note: feed_data argument should be passed using base::Passed(&feed_data), not
// feed_data.Pass().
typedef base::Callback<void(GDataErrorCode error,
                            scoped_ptr<base::Value> feed_data)> GetDataCallback;

// This class performs the operation for fetching and converting the fetched
// content into a base::Value.
class GetDataOperation : public UrlFetchOperationBase {
 public:
  // |callback| must not be null.
  GetDataOperation(OperationRegistry* registry,
                   net::URLRequestContextGetter* url_request_context_getter,
                   const GetDataCallback& callback);
  virtual ~GetDataOperation();

  // Parses JSON response.
  void ParseResponse(GDataErrorCode fetch_error_code, const std::string& data);

 protected:
  // UrlFetchOperationBase overrides.
  virtual void ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;
  virtual void RunCallbackOnPrematureFailure(
      GDataErrorCode fetch_error_code) OVERRIDE;

 private:
  // Runs |callback_| with the given parameters.
  void RunCallbackOnSuccess(GDataErrorCode fetch_error_code,
                            scoped_ptr<base::Value> value);


  // Called when ParseJsonOnBlockingPool() is completed.
  void OnDataParsed(GDataErrorCode fetch_error_code,
                    scoped_ptr<base::Value> value);

  const GetDataCallback callback_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<GetDataOperation> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(GetDataOperation);
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_BASE_OPERATIONS_H_
