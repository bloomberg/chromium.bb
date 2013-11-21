// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GCP20_PROTOTYPE_PRINTER_H_
#define GCP20_PROTOTYPE_PRINTER_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "cloud_print/gcp20/prototype/cloud_print_requester.h"
#include "cloud_print/gcp20/prototype/cloud_print_xmpp_listener.h"
#include "cloud_print/gcp20/prototype/dns_sd_server.h"
#include "cloud_print/gcp20/prototype/print_job_handler.h"
#include "cloud_print/gcp20/prototype/printer_state.h"
#include "cloud_print/gcp20/prototype/privet_http_server.h"
#include "cloud_print/gcp20/prototype/x_privet_token.h"

extern const base::FilePath::CharType kPrinterStatePath[];

// This class maintains work of DNS-SD server, HTTP server and others.
class Printer : public base::SupportsWeakPtr<Printer>,
                public PrivetHttpServer::Delegate,
                public CloudPrintRequester::Delegate,
                public CloudPrintXmppListener::Delegate {
 public:
  // Constructs uninitialized object.
  Printer();

  // Destroys the object.
  virtual ~Printer();

  // Starts all servers.
  bool Start();

  // Returns true if printer was started.
  bool IsRunning() const;

  // Stops all servers.
  void Stop();

 private:
  FRIEND_TEST_ALL_PREFIXES(Printer, ValidateCapabilities);

  enum ConnectionState {
    NOT_CONFIGURED,
    OFFLINE,
    ONLINE,
    CONNECTING
  };

  std::string GetRawCdd();

  // PrivetHttpServer::Delegate methods:
  virtual PrivetHttpServer::RegistrationErrorStatus RegistrationStart(
      const std::string& user) OVERRIDE;
  virtual PrivetHttpServer::RegistrationErrorStatus RegistrationGetClaimToken(
      const std::string& user,
      std::string* token,
      std::string* claim_url) OVERRIDE;
  virtual PrivetHttpServer::RegistrationErrorStatus RegistrationComplete(
      const std::string& user,
      std::string* device_id) OVERRIDE;
  virtual PrivetHttpServer::RegistrationErrorStatus RegistrationCancel(
      const std::string& user) OVERRIDE;
  virtual void GetRegistrationServerError(std::string* description) OVERRIDE;
  virtual void CreateInfo(PrivetHttpServer::DeviceInfo* info) OVERRIDE;
  virtual bool IsRegistered() const OVERRIDE;
  virtual bool IsLocalPrintingAllowed() const OVERRIDE;
  virtual bool CheckXPrivetTokenHeader(const std::string& token) const OVERRIDE;
  virtual const base::DictionaryValue& GetCapabilities() OVERRIDE;
  virtual LocalPrintJob::CreateResult CreateJob(
      const std::string& ticket,
      std::string* job_id,
      int* expires_in,
      int* error_timeout,
      std::string* error_description) OVERRIDE;
  virtual LocalPrintJob::SaveResult SubmitDoc(
      const LocalPrintJob& job,
      std::string* job_id,
      int* expires_in,
      std::string* error_description,
      int* timeout) OVERRIDE;
  virtual LocalPrintJob::SaveResult SubmitDocWithId(
      const LocalPrintJob& job,
      const std::string& job_id,
      int* expires_in,
      std::string* error_description,
      int* timeout) OVERRIDE;
  virtual bool GetJobState(const std::string& id,
                           LocalPrintJob::Info* info) OVERRIDE;

  // CloudRequester::Delegate methods:
  virtual void OnRegistrationStartResponseParsed(
      const std::string& registration_token,
      const std::string& complete_invite_url,
      const std::string& device_id) OVERRIDE;
  virtual void OnRegistrationFinished(
      const std::string& refresh_token,
      const std::string& access_token,
      int access_token_expires_in_seconds) OVERRIDE;
  virtual void OnXmppJidReceived(const std::string& xmpp_jid) OVERRIDE;
  virtual void OnAccesstokenReceviced(const std::string& access_token,
                                      int expires_in_seconds) OVERRIDE;
  virtual void OnRegistrationError(const std::string& description) OVERRIDE;
  virtual void OnNetworkError() OVERRIDE;
  virtual void OnServerError(const std::string& description) OVERRIDE;
  virtual void OnAuthError() OVERRIDE;
  virtual std::string GetAccessToken() OVERRIDE;
  virtual void OnPrintJobsAvailable(
      const std::vector<cloud_print_response_parser::Job>& jobs) OVERRIDE;
  virtual void OnPrintJobDownloaded(
      const cloud_print_response_parser::Job& job) OVERRIDE;
  virtual void OnPrintJobDone() OVERRIDE;
  virtual void OnLocalSettingsReceived(
      LocalSettings::State state,
      const LocalSettings& settings) OVERRIDE;
  virtual void OnLocalSettingsUpdated() OVERRIDE;

  // CloudPrintXmppListener::Delegate methods:
  virtual void OnXmppConnected() OVERRIDE;
  virtual void OnXmppAuthError() OVERRIDE;
  virtual void OnXmppNetworkError() OVERRIDE;
  virtual void OnXmppNewPrintJob(const std::string& device_id) OVERRIDE;
  virtual void OnXmppNewLocalSettings(const std::string& device_id) OVERRIDE;
  virtual void OnXmppDeleteNotification(const std::string& device_id) OVERRIDE;

  // Method for trying to reconnecting to server on start or after network fail.
  void TryConnect();

  // Connects XMPP.
  void ConnectXmpp();

  // Method to handle pending events.
  // Do *NOT* call this method instantly. Only with |PostOnIdle|.
  void OnIdle();

  // Ask Cloud Print server for printjobs.
  void FetchPrintJobs();

  // Ask Cloud Print server for new local sendings.
  void GetLocalSettings();

  // Applies new local settings to printer.
  void ApplyLocalSettings(const LocalSettings& settings);

  // Used for erasing all printer info.
  void OnPrinterDeleted();

  // Saves |access_token| and calculates time for next update.
  void RememberAccessToken(const std::string& access_token,
                           int expires_in_seconds);

  // Sets registration state to error and adds description.
  void SetRegistrationError(const std::string& description);

  // Checks if register call is called correctly (|user| is correct,
  // error is no set etc). Returns |false| if error status is put into |status|.
  // Otherwise no error was occurred.
  PrivetHttpServer::RegistrationErrorStatus CheckCommonRegErrors(
      const std::string& user);

  // Checks if confirmation was received.
  void WaitUserConfirmation(base::Time valid_until);

  // Generates ProxyId for this device.
  std::string GenerateProxyId() const;

  // Creates data for DNS TXT respond.
  std::vector<std::string> CreateTxt() const;

  // Saving and loading registration info from file.
  void SaveToFile();
  bool LoadFromFile();

  // Adds |OnIdle| method to the MessageLoop.
  void PostOnIdle();

  // Registration timeout.
  void CheckRegistrationExpiration();

  // Delays expiration after user action.
  void UpdateRegistrationExpiration();

  // Deletes registration expiration at all.
  void InvalidateRegistrationExpiration();

  // Methods to start HTTP and DNS-SD servers. Return |true| if servers
  // were started. If failed neither HTTP nor DNS-SD server will be running.
  bool StartLocalDiscoveryServers();

  // Methods to start HTTP and DNS-SD servers. Return |true| if servers
  // were started.
  bool StartDnsServer();
  bool StartHttpServer();

  // Converts errors.
  PrivetHttpServer::RegistrationErrorStatus ConfirmationToRegistrationError(
      PrinterState::ConfirmationState state);

  std::string ConnectionStateToString(ConnectionState state) const;

  // Changes state to OFFLINE and posts TryReconnect.
  // For registration reconnect is instant every time.
  void FallOffline(bool instant_reconnect);

  // Changes state and update info in DNS server. Returns |true| if state
  // was changed (otherwise state was the same).
  bool ChangeState(ConnectionState new_state);

  // Contains printers workflow info.
  PrinterState state_;

  // Connection state of device.
  ConnectionState connection_state_;

  // Contains DNS-SD server.
  DnsSdServer dns_server_;

  // Contains Privet HTTP server.
  PrivetHttpServer http_server_;

  // Contains CloudPrint client.
  scoped_ptr<CloudPrintRequester> requester_;

  // XMPP Listener.
  scoped_ptr<CloudPrintXmppListener> xmpp_listener_;

  XPrivetToken xtoken_;

  scoped_ptr<PrintJobHandler> print_job_handler_;

  // Uses for calculating uptime.
  base::Time starttime_;

  // Uses to validate registration timeout.
  base::Time registration_expiration_;

  // Used for preventing two and more OnIdle posted in message loop.
  bool on_idle_posted_;

  // Contains |true| if Printer has to check pending local settings.
  bool pending_local_settings_check_;

  // Contains |true| if Printer has to check available printjobs.
  bool pending_print_jobs_check_;

  // Contains |true| if Printer has to be deleted.
  bool pending_deletion_;

  DISALLOW_COPY_AND_ASSIGN(Printer);
};

#endif  // GCP20_PROTOTYPE_PRINTER_H_

