// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_GCP20_PROTOTYPE_CLOUD_REQUESTER_H_
#define CLOUD_PRINT_GCP20_PROTOTYPE_CLOUD_REQUESTER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "cloud_print/gcp20/prototype/cloud_print_request.h"
#include "cloud_print/gcp20/prototype/cloud_print_response_parser.h"
#include "cloud_print/gcp20/prototype/local_settings.h"
#include "google_apis/gaia/gaia_oauth_client.h"

class CloudPrintURLRequestContextGetter;
class GURL;
class URLRequestContextGetter;

extern const char kCloudPrintUrl[];

// Class for requesting CloudPrint server and parsing responses.
class CloudPrintRequester : public base::SupportsWeakPtr<CloudPrintRequester>,
                            public gaia::GaiaOAuthClient::Delegate,
                            public CloudPrintRequest::Delegate {
 public:
  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}

    // Invoked when server respond for registration-start query and response is
    // successfully parsed.
    virtual void OnRegistrationStartResponseParsed(
        const std::string& registration_token,
        const std::string& complete_invite_url,
        const std::string& device_id) = 0;

    // Invoked when server responded for registration-getAuthCode query and
    // response is successfully parsed.
    virtual void OnRegistrationFinished(
        const std::string& refresh_token,
        const std::string& access_token,
        int access_token_expires_in_seconds) = 0;

    // Invoked when XMPP JID was received and it has to be saved.
    virtual void OnXmppJidReceived(const std::string& xmpp_jid) = 0;

    // Invoked when access_token was received after UpdateAccesstoken() call.
    virtual void OnAccesstokenReceviced(const std::string& access_token,
                                        int expires_in_seconds) = 0;

    // Invoked when server respond with |"success" = false| or we cannot parse
    // response.
    virtual void OnRegistrationError(const std::string& description) = 0;

    // Invoked when network connection cannot be established.
    virtual void OnNetworkError() = 0;

    // Invoked when server error is received or cannot parse json response.
    virtual void OnServerError(const std::string& description) = 0;

    // Invoked when authorization failed.
    virtual void OnAuthError() = 0;

    // Invoked when access_token is needed.
    virtual std::string GetAccessToken() = 0;

    // Invoked when fetch response was received.
    virtual void OnPrintJobsAvailable(
        const std::vector<cloud_print_response_parser::Job>& jobs) = 0;

    // Invoked when printjob is finally downloaded and available for printing.
    virtual void OnPrintJobDownloaded(
        const cloud_print_response_parser::Job& job) = 0;

    // Invoked when printjob is marked as done on CloudPrint server.
    virtual void OnPrintJobDone() = 0;

    // Invoked when local settings response was received.
    virtual void OnLocalSettingsReceived(
        LocalSettings::State state,
        const LocalSettings& settings) = 0;

    // Invoked when CURRENT local settings was updated on server.
    virtual void OnLocalSettingsUpdated() = 0;
  };

  // Creates and initializes object.
  CloudPrintRequester(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                      Delegate* delegate);

  // Destroys the object.
  virtual ~CloudPrintRequester();

  // Returns |true| if either |gaia| or |request| is awaiting for response.
  bool IsBusy() const;

  // Creates query to server for starting registration.
  void StartRegistration(const std::string& proxy_id,
                         const std::string& device_name,
                         const std::string& user,
                         const LocalSettings& settings,
                         const std::string& cdd);

  // Creates request for completing registration and receiving refresh token.
  void CompleteRegistration();

  // Creates request for fetching printjobs.
  void FetchPrintJobs(const std::string& device_id);

  // Creates request for updating accesstoken.
  // TODO(maksymb): Handle expiration of accesstoken.
  void UpdateAccesstoken(const std::string& refresh_token);

  // Creates chain of requests for requesting printjob.
  void RequestPrintJob(const cloud_print_response_parser::Job& job);

  // Reports server that printjob has been printed.
  void SendPrintJobDone(const std::string& job_id);

  // Requests /printer API to receive local settings.
  void RequestLocalSettings(const std::string& device_id);

  // Updates local settings on server.
  void SendLocalSettings(const std::string& device_id,
                         const LocalSettings& settings);

 private:
  typedef base::Callback<void(const std::string&)> ParserCallback;

  // CloudPrintRequester::Delegate methods:
  virtual void OnFetchComplete(const std::string& response) OVERRIDE;
  virtual void OnFetchError(const std::string& server_api,
                            int server_code,
                            int server_http_code) OVERRIDE;
  virtual void OnFetchTimeoutReached() OVERRIDE;

  // gaia::GaiaOAuthClient::Delegate methods:
  virtual void OnGetTokensResponse(const std::string& refresh_token,
                                   const std::string& access_token,
                                   int expires_in_seconds) OVERRIDE;
  virtual void OnRefreshTokenResponse(const std::string& access_token,
                                      int expires_in_seconds) OVERRIDE;
  virtual void OnOAuthError() OVERRIDE;
  virtual void OnNetworkError(int response_code) OVERRIDE;

  // Creates GET request.
  scoped_ptr<CloudPrintRequest> CreateGet(const GURL& url,
                                          const ParserCallback& callback);

  // Creates POST request.
  scoped_ptr<CloudPrintRequest> CreatePost(const GURL& url,
                                           const std::string& content,
                                           const std::string& mimetype,
                                           const ParserCallback& callback);

  // Deletes all info about current request.
  void EraseRequest();

  // Parses register-start server response.
  void ParseRegisterStart(const std::string& response);

  // Parses register-complete server response. Initializes gaia (OAuth client)
  // and receives refresh token.
  void ParseRegisterComplete(const std::string& response);

  // Parses fetch printjobs server response.
  void ParseFetch(const std::string& response);

  // Invoked after receiving printjob ticket.
  void ParseGetPrintJobTicket(const std::string& response);

  // Invoked after receiving printjob file.
  void ParseGetPrintJobData(const std::string& response);

  // Invoked after marking printjob as DONE.
  void ParsePrintJobDone(const std::string& response);

  // Invoked after marking printjob as IN_PROGRESS.
  void ParsePrintJobInProgress(const std::string& response);

  // Invoked after receiving local_settings.
  void ParseLocalSettings(const std::string& response);

  // Invoked after updating current local_settings.
  void ParseLocalSettingUpdated(const std::string& response);

  // |request| contains |NULL| if no server response is awaiting. Otherwise wait
  // until callback will be called will be called and close connection.
  scoped_ptr<CloudPrintRequest> request_;

  // Contains information about current printjob. Information is filled by
  // CloudPrint server responses.
  scoped_ptr<cloud_print_response_parser::Job> current_print_job_;

  // CloudPrint context getter.
  scoped_refptr<net::URLRequestContextGetter> context_getter_;

  // URL for completing registration and receiving OAuth account.
  std::string polling_url_;

  // OAuth client information (client_id, client_secret, etc).
  gaia::OAuthClientInfo oauth_client_info_;

  // OAuth client.
  scoped_ptr<gaia::GaiaOAuthClient> gaia_;

  ParserCallback parser_callback_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintRequester);
};

#endif  // CLOUD_PRINT_GCP20_PROTOTYPE_CLOUD_REQUESTER_H_

