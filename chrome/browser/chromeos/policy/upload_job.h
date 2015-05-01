// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_UPLOAD_JOB_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_UPLOAD_JOB_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace policy {

class DataSegment;

// |UploadJob| can be used to upload screenshots and logfiles to the cloud.
// Data is uploaded via a POST request of type "multipart/form-data". The class
// relies on |OAuth2TokenService| to acquire an access token with a sufficient
// scope. Data segments can be added to the upload queue using AddDataSegment()
// and the upload is started by calling Start(). Calls to AddDataSegment() are
// only allowed prior to the first call to Start().
class UploadJob : public OAuth2TokenService::Consumer,
                  public net::URLFetcherDelegate {
 public:
  // If the upload fails, the |Delegate|'s OnFailure() method is invoked with
  // one of these error codes.
  enum ErrorCode {
    CONTENT_ENCODING_ERROR = 0,  // Failed to encode content.
    NETWORK_ERROR = 1,           // Network failure.
    AUTHENTICATION_ERROR = 2,    // Authentication failure.
    SERVER_ERROR = 3             // Server returned error or malformed reply.
  };

  class Delegate {
   public:
    // When the upload finishes successfully, the OnSuccess() method is invoked.
    virtual void OnSuccess() = 0;

    // On upload failure, the OnFailure() method is invoked with an |ErrorCode|
    // indicating the reason for failure.
    virtual void OnFailure(ErrorCode error_code) = 0;

   protected:
    virtual ~Delegate();

   private:
    DISALLOW_ASSIGN(Delegate);
  };

  // |UploadJob| uses a |MimeBoundaryGenerator| to generate strings which mark
  // the boundaries between data segments.
  class MimeBoundaryGenerator {
   public:
    virtual ~MimeBoundaryGenerator();

    virtual std::string GenerateBoundary(size_t length) const = 0;

   private:
    DISALLOW_ASSIGN(MimeBoundaryGenerator);
  };

  // An implemenation of the |MimeBoundaryGenerator| which uses random
  // alpha-numeric characters to construct MIME boundaries.
  class RandomMimeBoundaryGenerator : public MimeBoundaryGenerator {
   public:
    ~RandomMimeBoundaryGenerator() override;

    // MimeBoundaryGenerator:
    std::string GenerateBoundary(size_t length) const override;
  };

  UploadJob(const GURL& upload_url,
            const std::string& account_id,
            const OAuth2TokenService::ScopeSet& scope_set,
            OAuth2TokenService* token_service,
            scoped_refptr<net::URLRequestContextGetter> url_context_getter,
            Delegate* delegate,
            scoped_ptr<MimeBoundaryGenerator> boundary_generator);
  ~UploadJob() override;

  // Adds one data segment to the |UploadJob|. A |DataSegment| corresponds to
  // one "Content-Disposition" in the "multipart" request. As per RFC 2388,
  // each content-disposition has a |name| field, which must be unique within a
  // given request. For file uploads, the original local file name may be
  // supplied as well as in the |filename| field. If |filename| references an
  // empty string, no |filename| header will be added for this data segment.
  bool AddDataSegment(const std::string& name,
                      const std::string& filename,
                      const std::map<std::string, std::string>& header_entries,
                      scoped_ptr<std::string> data);

  // Initiates the data upload and returns |true| if successfully started. A
  // second call to Start() on an already uploading |UploadJob| instance will
  // fail and Start() will return |false|. The ongoing upload is not affected
  // by this in any way.
  bool Start();

 private:
  // Indicates the current state of the |UploadJob|.
  enum State {
    IDLE,               // Start() has not been called.
    ACQUIRING_TOKEN,    // Trying to acquire the access token.
    PREPARING_CONTENT,  // Currently encoding the content.
    UPLOADING,          // Upload started.
    SUCCESS,            // Upload successfully completed.
    ERROR               // Upload failed.
  };

  // OAuth2TokenService::Consumer:
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // net::URLFetcherDelegate:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Requests an access token for the upload scope.
  void RequestAccessToken();

  // Dispatches POST request to URLFetcher.
  void StartUpload(const std::string& access_token);

  // Constructs the body of the POST request by concatenating the
  // |data_segments_|, separated by appropriate content-disposition headers and
  // a MIME boundary. Places the request body in |post_data_| and a copy of the
  // MIME boundary in |mime_boundary_|. Returns true on success. If |post_data_|
  // and |mime_boundary_| were set already, returns true immediately. In case of
  // an error, clears |post_data_| and |mime_boundary_| and returns false.
  bool SetUpMultipart();

  // Assembles the request and starts the |URLFetcher|. Fails if another upload
  // is still in progress or the content was not successfully encoded.
  void CreateAndStartURLFetcher(const std::string& access_token);

  // The URL to which the POST request should be directed.
  const GURL upload_url_;

  // The account ID that will be used for the access token fetch.
  const std::string account_id_;

  // The set of OAuth2 scopes for which an access token will be requested.
  const OAuth2TokenService::ScopeSet scope_set_;

  // The token service used to retrieve the access token.
  OAuth2TokenService* const token_service_;

  // This is used to initialize the net::URLFetcher object.
  const scoped_refptr<net::URLRequestContextGetter> url_context_getter_;

  // The delegate to be notified of events.
  Delegate* const delegate_;

  // An implementation of |MimeBoundaryGenerator|. This instance will be used to
  // generate MIME boundaries when assembling the multipart request in
  // SetUpMultipart().
  scoped_ptr<MimeBoundaryGenerator> boundary_generator_;

  // Current state the uploader is in.
  State state_;

  // Contains the cached MIME boundary.
  scoped_ptr<std::string> mime_boundary_;

  // Contains the cached, encoded post data.
  scoped_ptr<std::string> post_data_;

  // Keeps track of the number of retries.
  int retry_;

  // The OAuth request to receive the access token.
  scoped_ptr<OAuth2TokenService::Request> access_token_request_;

  // The OAuth access token.
  std::string access_token_;

  // Helper to upload the data.
  scoped_ptr<net::URLFetcher> upload_fetcher_;

  // The data chunks to be uploaded.
  ScopedVector<DataSegment> data_segments_;

  DISALLOW_COPY_AND_ASSIGN(UploadJob);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_UPLOAD_JOB_H_
