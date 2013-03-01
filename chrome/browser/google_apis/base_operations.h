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
#include "chrome/browser/google_apis/drive_upload_mode.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/google_apis/operation_registry.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace base {
class Value;
}  // namespace base

namespace net {
class IOBuffer;
class URLRequestContextGetter;
}  // namespace net

namespace google_apis {

// Callback used to pass parsed JSON from ParseJson(). If parsing error occurs,
// then the passed argument is null.
typedef base::Callback<void(scoped_ptr<base::Value> value)> ParseJsonCallback;

// Parses JSON passed in |json| on blocking pool. Runs |callback| on the calling
// thread when finished with either success or failure.
// The callback must not be null.
void ParseJson(const std::string& json, const ParseJsonCallback& callback);

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
      const base::FilePath& drive_file_path);
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
  // file path in their constructors, if they want to save the downloaded
  // content to a file at a specific path.
  void set_output_file_path(const base::FilePath& output_file_path) {
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
  base::FilePath output_file_path_;

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


//=========================== InitiateUploadOperation ==========================

// Callback type for DocumentServiceInterface::InitiateUpload.
typedef base::Callback<void(GDataErrorCode error,
                            const GURL& upload_url)> InitiateUploadCallback;

// This class provides base implementation for performing the operation for
// initiating the upload of a file.
// |callback| will be called with the obtained upload URL. The URL will be
// used with operations for resuming the file uploading.
//
// Here's the flow of uploading:
// 1) Get the upload URL with a class inheriting InitiateUploadOperationBase.
// 2) Upload the first 512KB (see kUploadChunkSize in drive_uploader.cc)
//    of the target file to the upload URL
// 3) If there is more data to upload, go to 2).
//
class InitiateUploadOperationBase : public UrlFetchOperationBase {
 protected:
  // |callback| will be called with the upload URL, where upload data is
  // uploaded to with ResumeUploadOperation.
  // |callback| must not be null.
  // |content_type| and |content_length| should be the attributes of the
  // uploading file.
  InitiateUploadOperationBase(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const InitiateUploadCallback& callback,
      const base::FilePath& drive_file_path,
      const std::string& content_type,
      int64 content_length);
  virtual ~InitiateUploadOperationBase();

  // UrlFetchOperationBase overrides.
  virtual void ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;
  virtual void NotifySuccessToOperationRegistry() OVERRIDE;
  virtual void RunCallbackOnPrematureFailure(GDataErrorCode code) OVERRIDE;
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;

 private:
  const InitiateUploadCallback callback_;
  const base::FilePath drive_file_path_;
  const std::string content_type_;
  const int64 content_length_;

  DISALLOW_COPY_AND_ASSIGN(InitiateUploadOperationBase);
};

//========================== UploadRangeOperationBase ==========================

// Struct for response to ResumeUpload and GetUploadStatus.
struct UploadRangeResponse {
  UploadRangeResponse();
  UploadRangeResponse(GDataErrorCode code,
                      int64 start_position_received,
                      int64 end_position_received);
  ~UploadRangeResponse();

  GDataErrorCode code;
  // The values of "Range" header returned from the server. The values are
  // used to continue uploading more data. These are set to -1 if an upload
  // is complete.
  // |start_position_received| is inclusive and |end_position_received| is
  // exclusive to follow the common C++ manner, although the response from
  // the server has "Range" header in inclusive format at both sides.
  int64 start_position_received;
  int64 end_position_received;
};

// Base class for a URL fetch request expecting the response containing the
// current uploading range. This class processes the response containing
// "Range" header and invoke OnRangeOperationComplete.
class UploadRangeOperationBase : public UrlFetchOperationBase {
 protected:
  // |upload_location| is the URL of where to upload the file to.
  // |drive_file_path| is the path to the file seen in the UI. Not necessary
  // for resuming an upload, but used for adding an entry to OperationRegistry.
  // TODO(satorux): Remove the drive file path hack. crbug.com/163296
  UploadRangeOperationBase(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const UploadMode upload_mode,
      const base::FilePath& drive_file_path,
      const GURL& upload_url);
  virtual ~UploadRangeOperationBase();

  // UrlFetchOperationBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual net::URLFetcher::RequestType GetRequestType() const OVERRIDE;
  virtual void ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;
  virtual void NotifyStartToOperationRegistry() OVERRIDE;
  virtual void NotifySuccessToOperationRegistry() OVERRIDE;
  virtual void RunCallbackOnPrematureFailure(GDataErrorCode code) OVERRIDE;

  // This method will be called when the operation is done, regardless of
  // whether it is succeeded or failed.
  //
  // 1) If there is more data to upload, |code| of |response| is set to
  // HTTP_RESUME_INCOMPLETE, and positions are set appropriately. Also, |value|
  // will be set to NULL.
  // 2) If the upload is complete, |code| is set to HTTP_CREATED for a new file
  // or HTTP_SUCCESS for an existing file. Positions are set to -1, and |value|
  // is set to a parsed JSON value representing the uploaded file.
  // 3) If a premature failure is found, |code| is set to a value representing
  // the situation. Positions are set to 0, and |value| is set to NULL.
  //
  // See also the comments for UploadRangeResponse.
  // Note: Subclasses should have responsibility to run some callback
  // in this method to notify the finish status to its clients (or ignore it
  // under its responsibility).
  virtual void OnRangeOperationComplete(
      const UploadRangeResponse& response, scoped_ptr<base::Value> value) = 0;

 private:
  // Called when ParseJson() is completed.
  void OnDataParsed(GDataErrorCode code, scoped_ptr<base::Value> value);

  const UploadMode upload_mode_;
  const base::FilePath drive_file_path_;
  const GURL upload_url_;

  bool last_chunk_completed_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<UploadRangeOperationBase> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(UploadRangeOperationBase);
};

//========================== ResumeUploadOperationBase =========================

// This class performs the operation for resuming the upload of a file.
// More specifically, this operation uploads a chunk of data carried in |buf|
// of ResumeUploadResponseBase. This class is designed to share the
// implementation of upload resuming between GData WAPI and Drive API v2.
// The subclasses should implement OnRangeOperationComplete inherited by
// UploadRangeOperationBase, because the type of the response should be
// different (although the format in the server response is JSON).
class ResumeUploadOperationBase : public UploadRangeOperationBase {
 protected:
  // |start_position| is the start of range of contents currently stored in
  // |buf|. |end_position| is the end of range of contents currently stared in
  // |buf|. This is exclusive. For instance, if you are to upload the first
  // 500 bytes of data, |start_position| is 0 and |end_position| is 500.
  // |content_length| and |content_type| are the length and type of the
  // file content to be uploaded respectively.
  // |buf| holds current content to be uploaded.
  // See also UploadRangeOperationBase's comment for remaining parameters
  // meaining.
  ResumeUploadOperationBase(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      UploadMode upload_mode,
      const base::FilePath& drive_file_path,
      const GURL& upload_location,
      int64 start_position,
      int64 end_position,
      int64 content_length,
      const std::string& content_type,
      const scoped_refptr<net::IOBuffer>& buf);
  virtual ~ResumeUploadOperationBase();

  // UrlFetchOperationBase overrides.
  virtual std::vector<std::string> GetExtraRequestHeaders() const OVERRIDE;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) OVERRIDE;

  // content::UrlFetcherDelegate overrides.
  virtual void OnURLFetchUploadProgress(const net::URLFetcher* source,
                                        int64 current, int64 total) OVERRIDE;

 private:
  // The parameters for the request. See ResumeUploadParams for the details.
  const int64 start_position_;
  const int64 end_position_;
  const int64 content_length_;
  const std::string content_type_;
  const scoped_refptr<net::IOBuffer> buf_;

  DISALLOW_COPY_AND_ASSIGN(ResumeUploadOperationBase);
};

//============================ DownloadFileOperation ===========================

// Callback type for DownloadHostedDocument/DownloadFile
// DocumentServiceInterface calls.
typedef base::Callback<void(GDataErrorCode error,
                            const base::FilePath& temp_file)>
    DownloadActionCallback;

// This class performs the operation for downloading of a given document/file.
class DownloadFileOperation : public UrlFetchOperationBase {
 public:
  // download_action_callback:
  //   This callback is called when the download is complete. Must not be null.
  //
  // get_content_callback:
  //   This callback is called when some part of the content is
  //   read. Used to read the download content progressively. May be null.
  //
  // download_url:
  //   Specifies the target file to download.
  //
  // drive_file_path:
  //   Specifies the drive path of the target file. Shown in UI.
  //   TODO(satorux): Remove the drive file path hack. crbug.com/163296
  //
  // output_file_path:
  //   Specifies the file path to save the downloaded file.
  //
  DownloadFileOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const DownloadActionCallback& download_action_callback,
      const GetContentCallback& get_content_callback,
      const GURL& download_url,
      const base::FilePath& drive_file_path,
      const base::FilePath& output_file_path);
  virtual ~DownloadFileOperation();

 protected:
  // UrlFetchOperationBase overrides.
  virtual GURL GetURL() const OVERRIDE;
  virtual void ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;
  virtual void RunCallbackOnPrematureFailure(GDataErrorCode code) OVERRIDE;

  // net::URLFetcherDelegate overrides.
  virtual void OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                          int64 current, int64 total) OVERRIDE;
  virtual bool ShouldSendDownloadData() OVERRIDE;
  virtual void OnURLFetchDownloadData(
      const net::URLFetcher* source,
      scoped_ptr<std::string> download_data) OVERRIDE;

 private:
  const DownloadActionCallback download_action_callback_;
  const GetContentCallback get_content_callback_;
  const GURL download_url_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFileOperation);
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_BASE_OPERATIONS_H_
