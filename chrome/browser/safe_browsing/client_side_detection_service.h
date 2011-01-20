// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper class which handles communication with the SafeBrowsing backends for
// client-side phishing detection.  This class can be used to get a file
// descriptor to the client-side phishing model and also to send a ping back to
// Google to verify if a particular site is really phishing or not.
//
// This class is not thread-safe and expects all calls to GetModelFile() and
// SendClientReportPhishingRequest() to be made on the UI thread.  We also
// expect that the calling thread runs a message loop and that there is a FILE
// thread running to execute asynchronous file operations.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_SERVICE_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/platform_file.h"
#include "base/ref_counted.h"
#include "base/scoped_callback_factory.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/safe_browsing/csd.pb.h"
#include "chrome/common/net/url_fetcher.h"
#include "googleurl/src/gurl.h"

class URLRequestContextGetter;

namespace net {
class URLRequestStatus;
}  // namespace net

namespace safe_browsing {

class ClientSideDetectionService : public URLFetcher::Delegate {
 public:
  typedef Callback1<base::PlatformFile>::Type OpenModelDoneCallback;

  typedef Callback2<GURL /* phishing URL */, bool /* is phishing */>::Type
      ClientReportPhishingRequestCallback;

  virtual ~ClientSideDetectionService();

  // Creates a client-side detection service and starts fetching the client-side
  // detection model if necessary.  The model will be stored in |model_path|.
  // The caller takes ownership of the object.  This function may return NULL.
  static ClientSideDetectionService* Create(
      const FilePath& model_path,
      URLRequestContextGetter* request_context_getter);

  // From the URLFetcher::Delegate interface.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  // Gets the model file descriptor once the model is ready and stored
  // on disk.  If there was an error the callback is called and the
  // platform file is set to kInvalidPlatformFileValue. The
  // ClientSideDetectionService takes ownership of the |callback|.
  // The callback is always called after GetModelFile() returns and on the
  // same thread as GetModelFile() was called.
  void GetModelFile(OpenModelDoneCallback* callback);

  // Sends a request to the SafeBrowsing servers with the potentially phishing
  // URL and the client-side phishing score.  The |phishing_url| scheme should
  // be HTTP.  This method takes ownership of the |callback| and calls it once
  // the result has come back from the server or if an error occurs during the
  // fetch.  If an error occurs the phishing verdict will always be false.  The
  // callback is always called after SendClientReportPhishingRequest() returns
  // and on the same thread as SendClientReportPhishingRequest() was called.
  void SendClientReportPhishingRequest(
      const GURL& phishing_url,
      double score,
      ClientReportPhishingRequestCallback* callback);

 private:
  friend class ClientSideDetectionServiceTest;

  enum ModelStatus {
    // It's unclear whether or not the model was already fetched.
    UNKNOWN_STATUS,
    // Model is fetched and is stored on disk.
    READY_STATUS,
    // Error occured during fetching or writing.
    ERROR_STATUS,
  };

  static const char kClientReportPhishingUrl[];
  static const char kClientModelUrl[];

  // Use Create() method to create an instance of this object.
  ClientSideDetectionService(const FilePath& model_path,
                             URLRequestContextGetter* request_context_getter);

  // Sets the model status and invokes all the pending callbacks in
  // |open_callbacks_| with the current |model_file_| as parameter.
  void SetModelStatus(ModelStatus status);

  // Called once the initial open() of the model file is done.  If the file
  // exists we're done and we can call all the pending callbacks.  If the
  // file doesn't exist this method will asynchronously fetch the model
  // from the server by invoking StartFetchingModel().
  void OpenModelFileDone(base::PlatformFileError error_code,
                         base::PassPlatformFile file,
                         bool created);

  // Callback that is invoked once the attempt to create the model
  // file on disk is done.  If the file was created successfully we
  // start writing the model to disk (asynchronously).  Otherwise, we
  // give up and send an invalid platform file to all the pending callbacks.
  void CreateModelFileDone(base::PlatformFileError error_code,
                           base::PassPlatformFile file,
                           bool created);

  // Callback is invoked once we're done writing the model file to disk.
  // If everything went well then |model_file_| is a valid platform file
  // that can be sent to all the pending callbacks.  If an error occurs
  // we give up and send an invalid platform file to all the pending callbacks.
  void WriteModelFileDone(base::PlatformFileError error_code,
                          int bytes_written);

  // Helper function which closes the |model_file_| if necessary.
  void CloseModelFile();

  // Starts preparing the request to be sent to the client-side detection
  // frontends.
  void StartClientReportPhishingRequest(
      const GURL& phishing_url,
      double score,
      ClientReportPhishingRequestCallback* callback);

  // Starts getting the model file.
  void StartGetModelFile(OpenModelDoneCallback* callback);

  // Called by OnURLFetchComplete to handle the response from fetching the
  // model.
  void HandleModelResponse(const URLFetcher* source,
                           const GURL& url,
                           const net::URLRequestStatus& status,
                           int response_code,
                           const ResponseCookies& cookies,
                           const std::string& data);

  // Called by OnURLFetchComplete to handle the server response from
  // sending the client-side phishing request.
  void HandlePhishingVerdict(const URLFetcher* source,
                             const GURL& url,
                             const net::URLRequestStatus& status,
                             int response_code,
                             const ResponseCookies& cookies,
                             const std::string& data);

  FilePath model_path_;
  ModelStatus model_status_;
  base::PlatformFile model_file_;
  scoped_ptr<URLFetcher> model_fetcher_;
  scoped_ptr<std::string> tmp_model_string_;
  std::vector<OpenModelDoneCallback*> open_callbacks_;

  // Map of client report phishing request to the corresponding callback that
  // has to be invoked when the request is done.
  struct ClientReportInfo;
  std::map<const URLFetcher*, ClientReportInfo*> client_phishing_reports_;

  // Used to asynchronously call the callbacks for GetModelFile and
  // SendClientReportPhishingRequest.
  ScopedRunnableMethodFactory<ClientSideDetectionService> method_factory_;

  // The client-side detection service object (this) might go away before some
  // of the callbacks are done (e.g., asynchronous file operations).  The
  // callback factory will revoke all pending callbacks if this goes away to
  // avoid a crash.
  base::ScopedCallbackFactory<ClientSideDetectionService> callback_factory_;

  // The context we use to issue network requests.
  scoped_refptr<URLRequestContextGetter> request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(ClientSideDetectionService);
};

}  // namepsace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_SERVICE_H_
