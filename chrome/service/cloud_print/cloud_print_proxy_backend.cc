// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_proxy_backend.h"

#include <map>
#include <vector>

#include "app/l10n_util.h"
#include "base/file_util.h"
#include "base/md5.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/service/cloud_print/cloud_print_consts.h"
#include "chrome/service/cloud_print/cloud_print_helpers.h"
#include "chrome/service/cloud_print/cloud_print_url_fetcher.h"
#include "chrome/service/cloud_print/printer_job_handler.h"
#include "chrome/service/gaia/service_gaia_authenticator.h"
#include "chrome/service/service_process.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/listener/push_notifications_thread.h"
#include "jingle/notifier/listener/talk_mediator_impl.h"
#include "net/url_request/url_request_status.h"

// The real guts of CloudPrintProxyBackend, to keep the public client API clean.
class CloudPrintProxyBackend::Core
    : public base::RefCountedThreadSafe<CloudPrintProxyBackend::Core>,
      public CloudPrintURLFetcherDelegate,
      public cloud_print::PrintServerWatcherDelegate,
      public PrinterJobHandlerDelegate,
      public notifier::TalkMediator::Delegate {
 public:
  // It is OK for print_server_url to be empty. In this case system should
  // use system default (local) print server.
  explicit Core(CloudPrintProxyBackend* backend,
                const GURL& cloud_print_server_url,
                const DictionaryValue* print_system_settings);

  // Note:
  //
  // The Do* methods are the various entry points from CloudPrintProxyBackend
  // It calls us on a dedicated thread to actually perform synchronous
  // (and potentially blocking) operations.
  //
  // Called on the CloudPrintProxyBackend core_thread_ to perform
  // initialization. When we are passed in an LSID we authenticate using that
  // and retrieve new auth tokens.
  void DoInitializeWithLsid(const std::string& lsid,
                            const std::string& proxy_id);
  void DoInitializeWithToken(const std::string cloud_print_token,
                             const std::string cloud_print_xmpp_token,
                             const std::string email,
                             const std::string& proxy_id);

  // Called on the CloudPrintProxyBackend core_thread_ to perform
  // shutdown.
  void DoShutdown();
  void DoRegisterSelectedPrinters(
      const printing::PrinterList& printer_list);

  // CloudPrintURLFetcher::Delegate implementation.
  virtual CloudPrintURLFetcher::ResponseAction HandleJSONData(
      const URLFetcher* source,
      const GURL& url,
      DictionaryValue* json_data,
      bool succeeded);

  virtual void OnRequestAuthError();

  // cloud_print::PrintServerWatcherDelegate implementation
  virtual void OnPrinterAdded();
  // PrinterJobHandler::Delegate implementation
  virtual void OnPrinterJobHandlerShutdown(PrinterJobHandler* job_handler,
                                   const std::string& printer_id);
  virtual void OnAuthError();

  // notifier::TalkMediator::Delegate implementation.
  virtual void OnNotificationStateChange(
      bool notifications_enabled);
  virtual void OnIncomingNotification(
      const IncomingNotificationData& notification_data);
  virtual void OnOutgoingNotification();

 private:
  // Prototype for a response handler.
  typedef CloudPrintURLFetcher::ResponseAction
      (CloudPrintProxyBackend::Core::*ResponseHandler)(
          const URLFetcher* source,
          const GURL& url,
          DictionaryValue* json_data,
          bool succeeded);
  // Begin response handlers
  CloudPrintURLFetcher::ResponseAction HandlePrinterListResponse(
      const URLFetcher* source,
      const GURL& url,
      DictionaryValue* json_data,
      bool succeeded);

  CloudPrintURLFetcher::ResponseAction HandleRegisterPrinterResponse(
      const URLFetcher* source,
      const GURL& url,
      DictionaryValue* json_data,
      bool succeeded);

  CloudPrintURLFetcher::ResponseAction HandleRegisterFailedStatusResponse(
      const URLFetcher* source,
      const GURL& url,
      DictionaryValue* json_data,
      bool succeeded);

CloudPrintURLFetcher::ResponseAction HandlePrintSystemUnavailableResponse(
      const URLFetcher* source,
      const GURL& url,
      DictionaryValue* json_data,
      bool succeeded);
  // End response handlers

  // NotifyXXX is how the Core communicates with the frontend across
  // threads.
  void NotifyPrinterListAvailable(
      const printing::PrinterList& printer_list);
  void NotifyAuthenticated(
    const std::string& cloud_print_token,
    const std::string& cloud_print_xmpp_token,
    const std::string& email);
  void NotifyAuthenticationFailed();
  void NotifyPrintSystemUnavailable();

  // Starts a new printer registration process.
  void StartRegistration();
  // Ends the printer registration process.
  void EndRegistration();
  // Registers printer capabilities and defaults for the next printer in the
  // list with the cloud print server.
  void RegisterNextPrinter();
  // Retrieves the list of registered printers for this user/proxy combination
  // from the cloud print server.
  void GetRegisteredPrinters();
  // Removes the given printer from the list. Returns false if the printer
  // did not exist in the list.
  bool RemovePrinterFromList(const std::string& printer_name);
  // Initializes a job handler object for the specified printer. The job
  // handler is responsible for checking for pending print jobs for this
  // printer and print them.
  void InitJobHandlerForPrinter(DictionaryValue* printer_data);
  // Sends a diagnostic message to the cloud print server that the print
  // system failed to initialize.
  void ReportPrintSystemUnavailable(const std::string& failure_message);

  // Callback method for GetPrinterCapsAndDefaults.
  void OnReceivePrinterCaps(
      bool succeeded,
      const std::string& printer_name,
      const printing::PrinterCapsAndDefaults& caps_and_defaults);

  void HandlePrinterNotification(const std::string& printer_id);
  void PollForJobs();
  // Schedules a task to poll for jobs. Does nothing if a task is already
  // scheduled.
  void ScheduleJobPoll();

  // Our parent CloudPrintProxyBackend
  CloudPrintProxyBackend* backend_;

  GURL cloud_print_server_url_;
  scoped_ptr<DictionaryValue> print_system_settings_;
  // Pointer to current print system.
  scoped_refptr<cloud_print::PrintSystem> print_system_;
  // The list of printers to be registered with the cloud print server.
  // To begin with,this list is initialized with the list of local and network
  // printers available. Then we query the server for the list of printers
  // already registered. We trim this list to remove the printers already
  // registered. We then pass a copy of this list to the frontend to give the
  // user a chance to further trim the list. When the frontend gives us the
  // final list we make a copy into this so that we can start registering.
  printing::PrinterList printer_list_;
  // The CloudPrintURLFetcher instance for the current request.
  scoped_refptr<CloudPrintURLFetcher> request_;
  // The index of the nex printer to be uploaded.
  size_t next_upload_index_;
  // The unique id for this proxy
  std::string proxy_id_;
  // The GAIA auth token
  std::string auth_token_;
  // Cached info about the last printer that we tried to upload. We cache this
  // so we won't have to requery the printer if the upload fails and we need
  // to retry.
  std::string last_uploaded_printer_name_;
  printing::PrinterCapsAndDefaults last_uploaded_printer_info_;
  // A map of printer id to job handler.
  typedef std::map<std::string, scoped_refptr<PrinterJobHandler> >
      JobHandlerMap;
  JobHandlerMap job_handler_map_;
  ResponseHandler next_response_handler_;
  scoped_refptr<cloud_print::PrintSystem::PrintServerWatcher>
      print_server_watcher_;
  bool new_printers_available_;
  // Notification (xmpp) handler.
  scoped_ptr<notifier::TalkMediator> talk_mediator_;
  // Indicates whether XMPP notifications are currently enabled.
  bool notifications_enabled_;
  // The time when notifications were enabled. Valid only when
  // notifications_enabled_ is true.
  base::TimeTicks notifications_enabled_since_;
  // Indicates whether a task to poll for jobs has been scheduled.
  bool job_poll_scheduled_;
  // The channel we are interested in receiving push notifications for.
  // This is "cloudprint.google.com/proxy/<proxy_id>"
  std::string push_notifications_channel_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

CloudPrintProxyBackend::CloudPrintProxyBackend(
    CloudPrintProxyFrontend* frontend,
    const GURL& cloud_print_server_url,
    const DictionaryValue* print_system_settings)
      : core_thread_("Chrome_CloudPrintProxyCoreThread"),
        frontend_loop_(MessageLoop::current()),
        frontend_(frontend) {
  DCHECK(frontend_);
  core_ = new Core(this, cloud_print_server_url, print_system_settings);
}

CloudPrintProxyBackend::~CloudPrintProxyBackend() {
  DCHECK(!core_);
}

bool CloudPrintProxyBackend::InitializeWithLsid(const std::string& lsid,
                                                const std::string& proxy_id) {
  if (!core_thread_.Start())
    return false;
  core_thread_.message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(
        core_.get(), &CloudPrintProxyBackend::Core::DoInitializeWithLsid, lsid,
        proxy_id));
  return true;
}

bool CloudPrintProxyBackend::InitializeWithToken(
    const std::string cloud_print_token,
    const std::string cloud_print_xmpp_token,
    const std::string email,
    const std::string& proxy_id) {
  if (!core_thread_.Start())
    return false;
  core_thread_.message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(
        core_.get(), &CloudPrintProxyBackend::Core::DoInitializeWithToken,
        cloud_print_token, cloud_print_xmpp_token, email, proxy_id));
  return true;
}

void CloudPrintProxyBackend::Shutdown() {
  core_thread_.message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(core_.get(),
                        &CloudPrintProxyBackend::Core::DoShutdown));
  core_thread_.Stop();
  core_ = NULL;  // Releases reference to core_.
}

void CloudPrintProxyBackend::RegisterPrinters(
    const printing::PrinterList& printer_list) {
  core_thread_.message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(
          core_.get(),
          &CloudPrintProxyBackend::Core::DoRegisterSelectedPrinters,
          printer_list));
}

CloudPrintProxyBackend::Core::Core(CloudPrintProxyBackend* backend,
                                   const GURL& cloud_print_server_url,
                                   const DictionaryValue* print_system_settings)
    : backend_(backend),
      cloud_print_server_url_(cloud_print_server_url),
      next_upload_index_(0),
      next_response_handler_(NULL),
      new_printers_available_(false),
      notifications_enabled_(false),
      job_poll_scheduled_(false) {
  if (print_system_settings) {
    // It is possible to have no print settings specified.
    print_system_settings_.reset(print_system_settings->DeepCopy());
  }
}

void CloudPrintProxyBackend::Core::DoInitializeWithLsid(
    const std::string& lsid, const std::string& proxy_id) {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  // Since Talk does not accept a Cloud Print token, for now, we make 2 auth
  // requests, one for the chromiumsync service and another for print. This is
  // temporary and should be removed once Talk supports our token.
  // Note: The GAIA login is synchronous but that should be OK because we are in
  // the CloudPrintProxyCoreThread and we cannot really do anything else until
  // the GAIA signin is successful.
  std::string user_agent = "ChromiumBrowser";
  scoped_refptr<ServiceGaiaAuthenticator> gaia_auth_for_talk(
      new ServiceGaiaAuthenticator(
          user_agent, kSyncGaiaServiceId, kGaiaUrl,
          g_service_process->io_thread()->message_loop_proxy()));
  gaia_auth_for_talk->set_message_loop(MessageLoop::current());
  bool auth_succeeded = false;
  if (gaia_auth_for_talk->AuthenticateWithLsid(lsid)) {
    scoped_refptr<ServiceGaiaAuthenticator> gaia_auth_for_print(
        new ServiceGaiaAuthenticator(
            user_agent, kCloudPrintGaiaServiceId, kGaiaUrl,
            g_service_process->io_thread()->message_loop_proxy()));
    gaia_auth_for_print->set_message_loop(MessageLoop::current());
    if (gaia_auth_for_print->AuthenticateWithLsid(lsid)) {
      auth_succeeded = true;
      // Let the frontend know that we have authenticated.
      backend_->frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
          &Core::NotifyAuthenticated, gaia_auth_for_print->auth_token(),
          gaia_auth_for_talk->auth_token(), gaia_auth_for_talk->email()));
      backend_->frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
          &Core::NotifyAuthenticated, gaia_auth_for_print->auth_token(),
          gaia_auth_for_talk->auth_token(), gaia_auth_for_talk->email()));
      DoInitializeWithToken(gaia_auth_for_print->auth_token(),
                            gaia_auth_for_talk->auth_token(),
                            gaia_auth_for_talk->email(),
                            proxy_id);
    }
  }

  if (!auth_succeeded) {
    // Let the frontend know the of authentication failure.
    backend_->frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
        &Core::NotifyAuthenticationFailed));
  }
}

void CloudPrintProxyBackend::Core::DoInitializeWithToken(
    const std::string cloud_print_token,
    const std::string cloud_print_xmpp_token,
    const std::string email, const std::string& proxy_id) {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  VLOG(1) << "CP_PROXY: Starting proxy, id: " << proxy_id;

  print_system_ =
      cloud_print::PrintSystem::CreateInstance(print_system_settings_.get());
  if (!print_system_.get()) {
    NOTREACHED();
    return;  // No print system available, fail initalization.
  }

  cloud_print::PrintSystem::PrintSystemResult result = print_system_->Init();

  // TODO(sanjeevr): Validate the tokens.
  auth_token_ = cloud_print_token;

  if (result.succeeded()) {
    const notifier::NotifierOptions kNotifierOptions;
    const bool kInvalidateXmppAuthToken = false;
    const bool kAllowInsecureXmppConnection = false;
    talk_mediator_.reset(new notifier::TalkMediatorImpl(
        new notifier::PushNotificationsThread(
            kNotifierOptions,
            kCloudPrintPushNotificationsSource),
        kInvalidateXmppAuthToken,
        kAllowInsecureXmppConnection));
    push_notifications_channel_ = kCloudPrintPushNotificationsSource;
    push_notifications_channel_.append("/proxy/");
    push_notifications_channel_.append(proxy_id);
    talk_mediator_->AddSubscribedServiceUrl(push_notifications_channel_);
    talk_mediator_->SetDelegate(this);
    talk_mediator_->SetAuthToken(email, cloud_print_xmpp_token,
                                 kSyncGaiaServiceId);
    talk_mediator_->Login();

    print_server_watcher_ = print_system_->CreatePrintServerWatcher();
    print_server_watcher_->StartWatching(this);

    proxy_id_ = proxy_id;

    StartRegistration();
  } else {
    // We could not initialize the print system. We need to notify the server.
    ReportPrintSystemUnavailable(result.message());
  }
}

void CloudPrintProxyBackend::Core::StartRegistration() {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  printer_list_.clear();
  print_system_->EnumeratePrinters(&printer_list_);
  // Now we need to ask the server about printers that were registered on the
  // server so that we can trim this list.
  GetRegisteredPrinters();
}

void CloudPrintProxyBackend::Core::EndRegistration() {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  request_ = NULL;
  if (new_printers_available_) {
    new_printers_available_ = false;
    StartRegistration();
  }
}

void CloudPrintProxyBackend::Core::DoShutdown() {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  VLOG(1) << "CP_PROXY: Shutdown proxy, id: " << proxy_id_;
  if (print_server_watcher_ != NULL)
    print_server_watcher_->StopWatching();

  // Need to kill all running jobs.
  while (!job_handler_map_.empty()) {
    JobHandlerMap::iterator index = job_handler_map_.begin();
    // Shutdown will call our OnPrinterJobHandlerShutdown method which will
    // remove this from the map.
    index->second->Shutdown();
  }
  // Important to delete the TalkMediator on this thread.
  talk_mediator_.reset();
  notifications_enabled_ = false;
  notifications_enabled_since_ = base::TimeTicks();
  request_ = NULL;
}

void CloudPrintProxyBackend::Core::DoRegisterSelectedPrinters(
    const printing::PrinterList& printer_list) {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  if (!print_system_.get())
    return;  // No print system available.
  printer_list_.assign(printer_list.begin(), printer_list.end());
  next_upload_index_ = 0;
  RegisterNextPrinter();
}

void CloudPrintProxyBackend::Core::GetRegisteredPrinters() {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  GURL printer_list_url =
      CloudPrintHelpers::GetUrlForPrinterList(cloud_print_server_url_,
                                              proxy_id_);
  next_response_handler_ =
      &CloudPrintProxyBackend::Core::HandlePrinterListResponse;
  request_ = new CloudPrintURLFetcher;
  request_->StartGetRequest(printer_list_url, this, auth_token_,
                            kCloudPrintAPIMaxRetryCount);
}

void CloudPrintProxyBackend::Core::RegisterNextPrinter() {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  // For the next printer to be uploaded, create a multi-part post request to
  // upload the printer capabilities and the printer defaults.
  if (next_upload_index_ < printer_list_.size()) {
    const printing::PrinterBasicInfo& info =
        printer_list_.at(next_upload_index_);
    // If we are retrying a previous upload, we don't need to fetch the caps
    // and defaults again.
    if (info.printer_name != last_uploaded_printer_name_) {
      cloud_print::PrintSystem::PrinterCapsAndDefaultsCallback* callback =
           NewCallback(this,
                       &CloudPrintProxyBackend::Core::OnReceivePrinterCaps);
        // Asnchronously fetch the printer caps and defaults. The story will
        // continue in OnReceivePrinterCaps.
        print_system_->GetPrinterCapsAndDefaults(
            info.printer_name.c_str(), callback);
    } else {
      OnReceivePrinterCaps(true,
                           last_uploaded_printer_name_,
                           last_uploaded_printer_info_);
    }
  } else {
    EndRegistration();
  }
}

void CloudPrintProxyBackend::Core::OnReceivePrinterCaps(
    bool succeeded,
    const std::string& printer_name,
    const printing::PrinterCapsAndDefaults& caps_and_defaults) {
  DCHECK(next_upload_index_ < printer_list_.size());
  std::string mime_boundary;
  CloudPrintHelpers::CreateMimeBoundaryForUpload(&mime_boundary);
  std::string post_data;
  GURL post_url;
  if (succeeded) {
    const printing::PrinterBasicInfo& info =
        printer_list_.at(next_upload_index_);

    last_uploaded_printer_name_ = info.printer_name;
    last_uploaded_printer_info_ = caps_and_defaults;

    CloudPrintHelpers::AddMultipartValueForUpload(kProxyIdValue, proxy_id_,
                                                  mime_boundary,
                                                  std::string(), &post_data);
    CloudPrintHelpers::AddMultipartValueForUpload(kPrinterNameValue,
                                                  info.printer_name,
                                                  mime_boundary,
                                                  std::string(), &post_data);
    CloudPrintHelpers::AddMultipartValueForUpload(kPrinterDescValue,
                                                  info.printer_description,
                                                  mime_boundary,
                                                  std::string() , &post_data);
    CloudPrintHelpers::AddMultipartValueForUpload(
        kPrinterStatusValue, StringPrintf("%d", info.printer_status),
        mime_boundary, std::string(), &post_data);
    // Add printer options as tags.
    CloudPrintHelpers::GenerateMultipartPostDataForPrinterTags(info.options,
                                                               mime_boundary,
                                                               &post_data);

    CloudPrintHelpers::AddMultipartValueForUpload(
        kPrinterCapsValue, last_uploaded_printer_info_.printer_capabilities,
        mime_boundary, last_uploaded_printer_info_.caps_mime_type,
        &post_data);
    CloudPrintHelpers::AddMultipartValueForUpload(
        kPrinterDefaultsValue, last_uploaded_printer_info_.printer_defaults,
        mime_boundary, last_uploaded_printer_info_.defaults_mime_type,
        &post_data);
    // Send a hash of the printer capabilities to the server. We will use this
    // later to check if the capabilities have changed
    CloudPrintHelpers::AddMultipartValueForUpload(
        kPrinterCapsHashValue,
        MD5String(last_uploaded_printer_info_.printer_capabilities),
        mime_boundary, std::string(), &post_data);
    post_url = CloudPrintHelpers::GetUrlForPrinterRegistration(
        cloud_print_server_url_);

    next_response_handler_ =
        &CloudPrintProxyBackend::Core::HandleRegisterPrinterResponse;
  } else {
    LOG(ERROR) << "CP_PROXY: Failed to get printer info for: " <<
        printer_name;
    // This printer failed to register, notify the server of this failure.
    post_url = CloudPrintHelpers::GetUrlForUserMessage(
        cloud_print_server_url_,
        kGetPrinterCapsFailedMessageId);
    string16 printer_name_utf16 = UTF8ToUTF16(printer_name);
    std::string status_message = l10n_util::GetStringFUTF8(
        IDS_CLOUD_PRINT_REGISTER_PRINTER_FAILED,
        printer_name_utf16);
    CloudPrintHelpers::AddMultipartValueForUpload(kMessageTextValue,
                                                  status_message,
                                                  mime_boundary,
                                                  std::string(),
                                                  &post_data);
    next_response_handler_ =
        &CloudPrintProxyBackend::Core::HandleRegisterFailedStatusResponse;
  }
  // Terminate the request body
  post_data.append("--" + mime_boundary + "--\r\n");
  std::string mime_type("multipart/form-data; boundary=");
  mime_type += mime_boundary;
  request_ = new CloudPrintURLFetcher;
  request_->StartPostRequest(post_url, this, auth_token_,
                             kCloudPrintAPIMaxRetryCount, mime_type,
                             post_data);
}

void CloudPrintProxyBackend::Core::HandlePrinterNotification(
    const std::string& printer_id) {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  VLOG(1) << "CP_PROXY: Handle printer notification, id: " << printer_id;
  JobHandlerMap::iterator index = job_handler_map_.find(printer_id);
  if (index != job_handler_map_.end())
    index->second->CheckForJobs(kJobFetchReasonNotified);
}

void CloudPrintProxyBackend::Core::PollForJobs() {
  VLOG(1) << "CP_PROXY: Polling for jobs.";
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  for (JobHandlerMap::iterator index = job_handler_map_.begin();
       index != job_handler_map_.end(); index++) {
    // If notifications are on, then we should poll for this printer only if
    // the last time it fetched jobs was before notifications were last enabled.
    bool should_poll =
        !notifications_enabled_ ||
        (index->second->last_job_fetch_time() <= notifications_enabled_since_);
    if (should_poll)
      index->second->CheckForJobs(kJobFetchReasonPoll);
  }
  job_poll_scheduled_ = false;
  // If we don't have notifications, poll again after a while.
  if (!notifications_enabled_)
    ScheduleJobPoll();
}

void CloudPrintProxyBackend::Core::ScheduleJobPoll() {
  if (!job_poll_scheduled_) {
    int interval_in_seconds = base::RandInt(kMinJobPollIntervalSecs,
                                            kMaxJobPollIntervalSecs);
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        NewRunnableMethod(this, &CloudPrintProxyBackend::Core::PollForJobs),
        interval_in_seconds * 1000);
    job_poll_scheduled_ = true;
  }
}

// CloudPrintURLFetcher::Delegate implementation.
CloudPrintURLFetcher::ResponseAction
CloudPrintProxyBackend::Core::HandleJSONData(
    const URLFetcher* source,
    const GURL& url,
    DictionaryValue* json_data,
    bool succeeded) {
  DCHECK(next_response_handler_);
  return (this->*next_response_handler_)(source, url, json_data, succeeded);
}

void CloudPrintProxyBackend::Core::OnRequestAuthError() {
  OnAuthError();
}

void CloudPrintProxyBackend::Core::NotifyPrinterListAvailable(
    const printing::PrinterList& printer_list) {
  DCHECK(MessageLoop::current() == backend_->frontend_loop_);
  backend_->frontend_->OnPrinterListAvailable(printer_list);
}

void CloudPrintProxyBackend::Core::NotifyAuthenticated(
    const std::string& cloud_print_token,
    const std::string& cloud_print_xmpp_token,
    const std::string& email) {
  DCHECK(MessageLoop::current() == backend_->frontend_loop_);
  backend_->frontend_->OnAuthenticated(cloud_print_token,
                                       cloud_print_xmpp_token, email);
}

void CloudPrintProxyBackend::Core::NotifyAuthenticationFailed() {
  DCHECK(MessageLoop::current() == backend_->frontend_loop_);
  backend_->frontend_->OnAuthenticationFailed();
}

void CloudPrintProxyBackend::Core::NotifyPrintSystemUnavailable() {
  DCHECK(MessageLoop::current() == backend_->frontend_loop_);
  backend_->frontend_->OnPrintSystemUnavailable();
}

CloudPrintURLFetcher::ResponseAction
CloudPrintProxyBackend::Core::HandlePrinterListResponse(
    const URLFetcher* source,
    const GURL& url,
    DictionaryValue* json_data,
    bool succeeded) {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  if (!succeeded) {
    NOTREACHED();
    return CloudPrintURLFetcher::RETRY_REQUEST;
  }
  ListValue* printer_list = NULL;
  json_data->GetList(kPrinterListValue, &printer_list);
  // There may be no "printers" value in the JSON
  if (printer_list) {
    for (size_t index = 0; index < printer_list->GetSize(); index++) {
      DictionaryValue* printer_data = NULL;
      if (printer_list->GetDictionary(index, &printer_data)) {
        std::string printer_name;
        printer_data->GetString(kNameValue, &printer_name);
        RemovePrinterFromList(printer_name);
        InitJobHandlerForPrinter(printer_data);
      } else {
        NOTREACHED();
      }
    }
  }
  request_ = NULL;
  if (!printer_list_.empty()) {
    // Let the frontend know that we have a list of printers available.
    backend_->frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
        &Core::NotifyPrinterListAvailable, printer_list_));
  } else {
    // No more work to be done here.
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &Core::EndRegistration));
  }
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

void CloudPrintProxyBackend::Core::InitJobHandlerForPrinter(
    DictionaryValue* printer_data) {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  DCHECK(printer_data);
  PrinterJobHandler::PrinterInfoFromCloud printer_info_cloud;
  printer_data->GetString(kIdValue, &printer_info_cloud.printer_id);
  DCHECK(!printer_info_cloud.printer_id.empty());
  VLOG(1) << "CP_PROXY: Init job handler for printer id: "
          << printer_info_cloud.printer_id;
  JobHandlerMap::iterator index = job_handler_map_.find(
      printer_info_cloud.printer_id);
  // We might already have a job handler for this printer
  if (index == job_handler_map_.end()) {
    printing::PrinterBasicInfo printer_info;
    printer_data->GetString(kNameValue, &printer_info.printer_name);
    DCHECK(!printer_info.printer_name.empty());
    printer_data->GetString(kPrinterDescValue,
                            &printer_info.printer_description);
    // Printer status is a string value which actually contains an integer.
    std::string printer_status;
    if (printer_data->GetString(kPrinterStatusValue, &printer_status)) {
      base::StringToInt(printer_status, &printer_info.printer_status);
    }
    printer_data->GetString(kPrinterCapsHashValue,
        &printer_info_cloud.caps_hash);
    ListValue* tags_list = NULL;
    printer_data->GetList(kTagsValue, &tags_list);
    if (tags_list) {
      for (size_t index = 0; index < tags_list->GetSize(); index++) {
        std::string tag;
        tags_list->GetString(index, &tag);
        if (StartsWithASCII(tag, kTagsHashTagName, false)) {
          std::vector<std::string> tag_parts;
          base::SplitStringDontTrim(tag, '=', &tag_parts);
          DCHECK_EQ(tag_parts.size(), 2U);
          if (tag_parts.size() == 2)
            printer_info_cloud.tags_hash = tag_parts[1];
        }
      }
    }
    scoped_refptr<PrinterJobHandler> job_handler;
    job_handler = new PrinterJobHandler(printer_info, printer_info_cloud,
                                        auth_token_, cloud_print_server_url_,
                                        print_system_.get(), this);
    job_handler_map_[printer_info_cloud.printer_id] = job_handler;
    job_handler->Initialize();
  }
}

void CloudPrintProxyBackend::Core::ReportPrintSystemUnavailable(
    const std::string& failure_message) {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  std::string mime_boundary;
  CloudPrintHelpers::CreateMimeBoundaryForUpload(&mime_boundary);
  GURL post_url = CloudPrintHelpers::GetUrlForUserMessage(
      cloud_print_server_url_,
      kPrintSystemFailedMessageId);
  std::string post_data;
  CloudPrintHelpers::AddMultipartValueForUpload(kMessageTextValue,
                                                failure_message,
                                                mime_boundary,
                                                std::string(),
                                                &post_data);
  next_response_handler_ =
      &CloudPrintProxyBackend::Core::HandlePrintSystemUnavailableResponse;
  // Terminate the request body
  post_data.append("--" + mime_boundary + "--\r\n");
  std::string mime_type("multipart/form-data; boundary=");
  mime_type += mime_boundary;
  request_ = new CloudPrintURLFetcher;
  request_->StartPostRequest(post_url, this, auth_token_,
                             kCloudPrintAPIMaxRetryCount, mime_type,
                             post_data);
}

CloudPrintURLFetcher::ResponseAction
CloudPrintProxyBackend::Core::HandleRegisterPrinterResponse(
    const URLFetcher* source,
    const GURL& url,
    DictionaryValue* json_data,
    bool succeeded) {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  if (succeeded) {
    ListValue* printer_list = NULL;
    json_data->GetList(kPrinterListValue, &printer_list);
    // There should be a "printers" value in the JSON
    DCHECK(printer_list);
    if (printer_list) {
      DictionaryValue* printer_data = NULL;
      if (printer_list->GetDictionary(0, &printer_data))
        InitJobHandlerForPrinter(printer_data);
    }
  }
  next_upload_index_++;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &CloudPrintProxyBackend::Core::RegisterNextPrinter));
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
CloudPrintProxyBackend::Core::HandleRegisterFailedStatusResponse(
    const URLFetcher* source,
    const GURL& url,
    DictionaryValue* json_data,
    bool succeeded) {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  next_upload_index_++;
  MessageLoop::current()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &CloudPrintProxyBackend::Core::RegisterNextPrinter));
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction
CloudPrintProxyBackend::Core::HandlePrintSystemUnavailableResponse(
    const URLFetcher* source,
    const GURL& url,
    DictionaryValue* json_data,
    bool succeeded) {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  // Let the frontend know that we do not have a print system.
  backend_->frontend_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &Core::NotifyPrintSystemUnavailable));
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

bool CloudPrintProxyBackend::Core::RemovePrinterFromList(
    const std::string& printer_name) {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  for (printing::PrinterList::iterator index = printer_list_.begin();
       index != printer_list_.end(); index++) {
    if (0 == base::strcasecmp(index->printer_name.c_str(),
                              printer_name.c_str())) {
      index = printer_list_.erase(index);
      return true;
    }
  }
  return false;
}

void CloudPrintProxyBackend::Core::OnNotificationStateChange(
    bool notification_enabled) {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  bool state_changed = (notification_enabled != notifications_enabled_);
  notifications_enabled_ = notification_enabled;
  if (notifications_enabled_) {
    notifications_enabled_since_ = base::TimeTicks::Now();
    VLOG(1) << "Notifications for proxy " << proxy_id_ << " were enabled at "
            << notifications_enabled_since_.ToInternalValue();
  } else {
    VLOG(1) << "Notifications for proxy " << proxy_id_ << " disabled.";
    notifications_enabled_since_ = base::TimeTicks();
  }
  // A state change means one of two cases.
  // Case 1: We just lost notifications. This this case we want to schedule a
  // job poll.
  // Case 2: Notifications just got re-enabled. In this case we want to schedule
  // a poll once for jobs we might have missed when we were dark. In reality
  // this is only needed when notifications get enabled for the first time. In
  // all other cases there would already be a scheduled task to poll in the
  // queue.
  // Note that ScheduleJobPoll will not schedule again if a job poll task is
  // already scheduled.
  if (state_changed)
    ScheduleJobPoll();
}


void CloudPrintProxyBackend::Core::OnIncomingNotification(
    const IncomingNotificationData& notification_data) {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  VLOG(1) << "CP_PROXY: Incoming notification.";
  if (0 == base::strcasecmp(push_notifications_channel_.c_str(),
                            notification_data.service_url.c_str()))
    HandlePrinterNotification(notification_data.service_specific_data);
}

void CloudPrintProxyBackend::Core::OnOutgoingNotification() {}

// cloud_print::PrinterChangeNotifier::Delegate implementation
void CloudPrintProxyBackend::Core::OnPrinterAdded() {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  if (request_.get())
    new_printers_available_ = true;
  else
    StartRegistration();
}

// PrinterJobHandler::Delegate implementation
void CloudPrintProxyBackend::Core::OnPrinterJobHandlerShutdown(
    PrinterJobHandler* job_handler, const std::string& printer_id) {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  VLOG(1) << "CP_PROXY: Printer job handle shutdown, id " << printer_id;
  job_handler_map_.erase(printer_id);
}

void CloudPrintProxyBackend::Core::OnAuthError() {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  VLOG(1) << "CP_PROXY: Auth Error";
  backend_->frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &Core::NotifyAuthenticationFailed));
}
