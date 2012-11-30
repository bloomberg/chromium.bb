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

namespace google_apis {

//======================= AuthenticatedOperationInterface ======================

// An interface class for implementing an operation which requires OAuth2
// authentication.
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
  // This function should be called before Start().
  // TODO(satorux): Make it a parameter of Start(). crbug.com/163535.
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
  // AuthenticatedOperationInterface overrides.
  virtual void Start(const std::string& auth_token,
                     const std::string& custom_user_agent) OVERRIDE;
  virtual void SetReAuthenticateCallback(
      const ReAuthenticateCallback& callback) OVERRIDE;
  virtual base::WeakPtr<AuthenticatedOperationInterface> GetWeakPtr() OVERRIDE;

 protected:
  explicit UrlFetchOperationBase(OperationRegistry* registry);
  // Use this constructor when you need to implement operations that take a
  // drive file path (ex. for downloading and uploading).
  // TODO(satorux): Remove the drive file path hack. crbug.com/163296
  UrlFetchOperationBase(OperationRegistry* registry,
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

  // The following members are used by DownloadFileOperation.
  // TODO(satorux): Make them private.
  bool save_temp_file_;
  FilePath output_file_path_;

 private:
  // OperationRegistry::Operation overrides.
  virtual void DoCancel() OVERRIDE;

  // URLFetcherDelegate overrides.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // AuthenticatedOperationInterface overrides.
  virtual void OnAuthFailed(GDataErrorCode code) OVERRIDE;

  ReAuthenticateCallback re_authenticate_callback_;
  int re_authenticate_count_;
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
  const EntryActionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(EntryActionOperation);
};

//============================== GetDataOperation ==============================

// Callback type for DocumentServiceInterface::GetDocuments.
// Note: feed_data argument should be passed using base::Passed(&feed_data), not
// feed_data.Pass().
typedef base::Callback<void(GDataErrorCode error,
                            scoped_ptr<base::Value> feed_data)> GetDataCallback;

// This class performs the operation for fetching and converting the fetched
// content into a base::Value.
class GetDataOperation : public UrlFetchOperationBase {
 public:
  GetDataOperation(OperationRegistry* registry,
                   const GetDataCallback& callback);
  virtual ~GetDataOperation();

  // Parses JSON response. A derived class should override this function if
  // the input data is not JSON (ex. XML).
  virtual void ParseResponse(GDataErrorCode fetch_error_code,
                             const std::string& data);

  // UrlFetchOperationBase overrides.
  virtual void ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;

  // Runs |callback_| with the given parameters, if |callback_| is not null.
  // TODO(satorux): Remove this by making |callback_| mandatory.
  void RunCallback(GDataErrorCode fetch_error_code,
                   scoped_ptr<base::Value> value);

 private:
  // UrlFetchOperationBase overrides.
  virtual void RunCallbackOnPrematureFailure(
      GDataErrorCode fetch_error_code) OVERRIDE;

  // Called when ParseJsonOnBlockingPool() is completed.
  void OnDataParsed(google_apis::GDataErrorCode fetch_error_code,
                    scoped_ptr<base::Value> value);

  const GetDataCallback callback_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<GetDataOperation> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(GetDataOperation);
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_BASE_OPERATIONS_H_
