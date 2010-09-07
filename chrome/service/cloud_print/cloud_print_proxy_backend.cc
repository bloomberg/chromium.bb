// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_proxy_backend.h"

#include "base/file_util.h"
#include "base/md5.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/service/cloud_print/cloud_print_consts.h"
#include "chrome/service/cloud_print/cloud_print_helpers.h"
#include "chrome/service/cloud_print/printer_job_handler.h"
#include "chrome/service/gaia/service_gaia_authenticator.h"
#include "chrome/service/service_process.h"
#include "jingle/notifier/listener/mediator_thread_impl.h"
#include "jingle/notifier/listener/talk_mediator_impl.h"

#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_status.h"

// The real guts of SyncBackendHost, to keep the public client API clean.
class CloudPrintProxyBackend::Core
    : public base::RefCountedThreadSafe<CloudPrintProxyBackend::Core>,
      public URLFetcherDelegate,
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
  // (and potentially blocking) syncapi operations.
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
      const cloud_print::PrinterList& printer_list);
  void DoHandlePrinterNotification(const std::string& printer_id);

  // URLFetcher::Delegate implementation.
  virtual void OnURLFetchComplete(const URLFetcher* source, const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);
  // cloud_print::PrintServerWatcherDelegate implementation
  virtual void OnPrinterAdded();
  // PrinterJobHandler::Delegate implementation
  void OnPrinterJobHandlerShutdown(PrinterJobHandler* job_handler,
                                   const std::string& printer_id);

  // notifier::TalkMediator::Delegate implementation.
  virtual void OnNotificationStateChange(
      bool notifications_enabled);

  virtual void OnIncomingNotification(
      const IncomingNotificationData& notification_data);

  virtual void OnOutgoingNotification();

 protected:
  // Prototype for a response handler.
  typedef void (CloudPrintProxyBackend::Core::*ResponseHandler)(
      const URLFetcher* source, const GURL& url,
      const URLRequestStatus& status, int response_code,
      const ResponseCookies& cookies, const std::string& data);
  // Begin response handlers
  void HandlePrinterListResponse(const URLFetcher* source, const GURL& url,
                                 const URLRequestStatus& status,
                                 int response_code,
                                 const ResponseCookies& cookies,
                                 const std::string& data);
  void HandleRegisterPrinterResponse(const URLFetcher* source,
                                     const GURL& url,
                                     const URLRequestStatus& status,
                                     int response_code,
                                     const ResponseCookies& cookies,
                                     const std::string& data);
  // End response handlers

  // NotifyXXX is how the Core communicates with the frontend across
  // threads.
  void NotifyPrinterListAvailable(
      const cloud_print::PrinterList& printer_list);
  void NotifyAuthenticated(
    const std::string& cloud_print_token,
    const std::string& cloud_print_xmpp_token,
    const std::string& email);
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
  void HandleServerError(Task* task_to_retry);
  // Removes the given printer from the list. Returns false if the printer
  // did not exist in the list.
  bool RemovePrinterFromList(const std::string& printer_name);
  // Initializes a job handler object for the specified printer. The job
  // handler is responsible for checking for pending print jobs for this
  // printer and print them.
  void InitJobHandlerForPrinter(DictionaryValue* printer_data);

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
  cloud_print::PrinterList printer_list_;
  // The URLFetcher instance for the current request
  scoped_ptr<URLFetcher> request_;
  // The index of the nex printer to be uploaded.
  size_t next_upload_index_;
  // The unique id for this proxy
  std::string proxy_id_;
  // The GAIA auth token
  std::string auth_token_;
  // The number of consecutive times that connecting to the server failed.
  int server_error_count_;
  // Cached info about the last printer that we tried to upload. We cache this
  // so we won't have to requery the printer if the upload fails and we need
  // to retry.
  std::string last_uploaded_printer_name_;
  cloud_print::PrinterCapsAndDefaults last_uploaded_printer_info_;
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
    const cloud_print::PrinterList& printer_list) {
  core_thread_.message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(
          core_.get(),
          &CloudPrintProxyBackend::Core::DoRegisterSelectedPrinters,
          printer_list));
}

void CloudPrintProxyBackend::HandlePrinterNotification(
    const std::string& printer_id) {
  core_thread_.message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(
          core_.get(),
          &CloudPrintProxyBackend::Core::DoHandlePrinterNotification,
          printer_id));
}

CloudPrintProxyBackend::Core::Core(CloudPrintProxyBackend* backend,
                                   const GURL& cloud_print_server_url,
                                   const DictionaryValue* print_system_settings)
    : backend_(backend), cloud_print_server_url_(cloud_print_server_url),
      next_upload_index_(0), server_error_count_(0),
      next_response_handler_(NULL), new_printers_available_(false) {
  if (print_system_settings) {
    // It is possible to have no print settings specified.
    print_system_settings_.reset(
        reinterpret_cast<DictionaryValue*>(print_system_settings->DeepCopy()));
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
  scoped_refptr<ServiceGaiaAuthenticator> gaia_auth_for_talk =
      new ServiceGaiaAuthenticator(
          user_agent, kSyncGaiaServiceId, kGaiaUrl,
          g_service_process->io_thread()->message_loop_proxy());
  gaia_auth_for_talk->set_message_loop(MessageLoop::current());
  // TODO(sanjeevr): Handle auth failure case. We basically need to disable
  // cloud print and shutdown.
  if (gaia_auth_for_talk->AuthenticateWithLsid(lsid)) {
    scoped_refptr<ServiceGaiaAuthenticator> gaia_auth_for_print =
        new ServiceGaiaAuthenticator(
            user_agent, kCloudPrintGaiaServiceId, kGaiaUrl,
            g_service_process->io_thread()->message_loop_proxy());
    gaia_auth_for_print->set_message_loop(MessageLoop::current());
    if (gaia_auth_for_print->AuthenticateWithLsid(lsid)) {
      // Let the frontend know that we have authenticated.
      backend_->frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
          &Core::NotifyAuthenticated, gaia_auth_for_print->auth_token(),
          gaia_auth_for_talk->auth_token(), gaia_auth_for_talk->email()));
      DoInitializeWithToken(gaia_auth_for_print->auth_token(),
                            gaia_auth_for_talk->auth_token(),
                            gaia_auth_for_talk->email(),
                            proxy_id);
    }
  }
}

void CloudPrintProxyBackend::Core::DoInitializeWithToken(
    const std::string cloud_print_token,
    const std::string cloud_print_xmpp_token,
    const std::string email, const std::string& proxy_id) {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  LOG(INFO) << "CP_PROXY: Starting proxy, id: " << proxy_id;

  print_system_ =
      cloud_print::PrintSystem::CreateInstance(print_system_settings_.get());
  if (!print_system_.get()) {
    NOTREACHED();
    return;  // No print system available, fail initalization.
  }

  // TODO(sanjeevr): Validate the tokens.
  auth_token_ = cloud_print_token;

  const bool kUseChromeAsyncSocket = true;
  const bool kTrySslTcpFirst = false;
  const bool kInitializeSsl = true;
  const bool kConnectImmediately = false;
  const bool kInvalidateXmppAuthToken = false;
  talk_mediator_.reset(new notifier::TalkMediatorImpl(
      new notifier::MediatorThreadImpl(kUseChromeAsyncSocket, kTrySslTcpFirst),
      kInitializeSsl, kConnectImmediately, kInvalidateXmppAuthToken));
  talk_mediator_->AddSubscribedServiceUrl(kCloudPrintTalkServiceUrl);
  talk_mediator_->SetDelegate(this);
  talk_mediator_->SetAuthToken(email, cloud_print_xmpp_token,
                               kSyncGaiaServiceId);
  talk_mediator_->Login();

  print_server_watcher_ = print_system_->CreatePrintServerWatcher();
  print_server_watcher_->StartWatching(this);

  proxy_id_ = proxy_id;
  StartRegistration();
}

void CloudPrintProxyBackend::Core::StartRegistration() {
  printer_list_.clear();
  print_system_->EnumeratePrinters(&printer_list_);
  server_error_count_ = 0;
  // Now we need to ask the server about printers that were registered on the
  // server so that we can trim this list.
  GetRegisteredPrinters();
}

void CloudPrintProxyBackend::Core::EndRegistration() {
  request_.reset();
  if (new_printers_available_) {
    new_printers_available_ = false;
    StartRegistration();
  }
}

void CloudPrintProxyBackend::Core::DoShutdown() {
  LOG(INFO) << "CP_PROXY: Shutdown proxy, id: " << proxy_id_;
  if (print_server_watcher_ != NULL)
    print_server_watcher_->StopWatching();

  // Need to kill all running jobs.
  while (!job_handler_map_.empty()) {
    JobHandlerMap::iterator index = job_handler_map_.begin();
    // Shutdown will call our OnPrinterJobHandlerShutdown method which will
    // remove this from the map.
    index->second->Shutdown();
  }
}

void CloudPrintProxyBackend::Core::DoRegisterSelectedPrinters(
    const cloud_print::PrinterList& printer_list) {
  if (!print_system_.get())
    return;  // No print system available.
  server_error_count_ = 0;
  printer_list_.assign(printer_list.begin(), printer_list.end());
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  next_upload_index_ = 0;
  RegisterNextPrinter();
}

void CloudPrintProxyBackend::Core::DoHandlePrinterNotification(
    const std::string& printer_id) {
  LOG(INFO) << "CP_PROXY: Handle printer notification, id: " << printer_id;
  JobHandlerMap::iterator index = job_handler_map_.find(printer_id);
  if (index != job_handler_map_.end())
    index->second->NotifyJobAvailable();
}

void CloudPrintProxyBackend::Core::GetRegisteredPrinters() {
  request_.reset(
      new URLFetcher(
          CloudPrintHelpers::GetUrlForPrinterList(cloud_print_server_url_,
                                                  proxy_id_),
          URLFetcher::GET, this));
  CloudPrintHelpers::PrepCloudPrintRequest(request_.get(), auth_token_);
  next_response_handler_ =
      &CloudPrintProxyBackend::Core::HandlePrinterListResponse;
  request_->Start();
}

void CloudPrintProxyBackend::Core::RegisterNextPrinter() {
  // For the next printer to be uploaded, create a multi-part post request to
  // upload the printer capabilities and the printer defaults.
  if (next_upload_index_ < printer_list_.size()) {
    const cloud_print::PrinterBasicInfo& info =
        printer_list_.at(next_upload_index_);
    bool have_printer_info = true;
    // If we are retrying a previous upload, we don't need to fetch the caps
    // and defaults again.
    if (info.printer_name != last_uploaded_printer_name_) {
      have_printer_info = print_system_->GetPrinterCapsAndDefaults(
          info.printer_name.c_str(), &last_uploaded_printer_info_);
    }
    if (have_printer_info) {
      last_uploaded_printer_name_ = info.printer_name;
      std::string mime_boundary;
      CloudPrintHelpers::CreateMimeBoundaryForUpload(&mime_boundary);
      std::string post_data;
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
      // Terminate the request body
      post_data.append("--" + mime_boundary + "--\r\n");
      std::string mime_type("multipart/form-data; boundary=");
      mime_type += mime_boundary;
      request_.reset(
          new URLFetcher(
              CloudPrintHelpers::GetUrlForPrinterRegistration(
                  cloud_print_server_url_),
              URLFetcher::POST, this));
      CloudPrintHelpers::PrepCloudPrintRequest(request_.get(), auth_token_);
      request_->set_upload_data(mime_type, post_data);
      next_response_handler_ =
          &CloudPrintProxyBackend::Core::HandleRegisterPrinterResponse;
      request_->Start();
    } else {
      LOG(ERROR) << "CP_PROXY: Failed to get printer info for: " <<
          info.printer_name;
      next_upload_index_++;
      MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(this,
          &CloudPrintProxyBackend::Core::RegisterNextPrinter));
    }
  } else {
    EndRegistration();
  }
}

// URLFetcher::Delegate implementation.
void CloudPrintProxyBackend::Core::OnURLFetchComplete(
    const URLFetcher* source, const GURL& url, const URLRequestStatus& status,
    int response_code, const ResponseCookies& cookies,
    const std::string& data) {
  DCHECK(source == request_.get());
  // We need a next response handler
  DCHECK(next_response_handler_);
  (this->*next_response_handler_)(source, url, status, response_code,
                                  cookies, data);
}

void CloudPrintProxyBackend::Core::NotifyPrinterListAvailable(
    const cloud_print::PrinterList& printer_list) {
  backend_->frontend_->OnPrinterListAvailable(printer_list);
}

void CloudPrintProxyBackend::Core::NotifyAuthenticated(
    const std::string& cloud_print_token,
    const std::string& cloud_print_xmpp_token,
    const std::string& email) {
  backend_->frontend_->OnAuthenticated(cloud_print_token,
                                       cloud_print_xmpp_token, email);
}

void CloudPrintProxyBackend::Core::HandlePrinterListResponse(
    const URLFetcher* source, const GURL& url, const URLRequestStatus& status,
    int response_code, const ResponseCookies& cookies,
    const std::string& data) {
  bool succeeded = false;
  if (status.is_success() && response_code == 200) {
    server_error_count_ = 0;
    // Parse the response JSON for the list of printers already registered.
    DictionaryValue* response_dict_temp = NULL;
    CloudPrintHelpers::ParseResponseJSON(data, &succeeded,
                                         &response_dict_temp);
    scoped_ptr<DictionaryValue> response_dict;
    response_dict.reset(response_dict_temp);
    if (succeeded) {
      DCHECK(response_dict.get());
      ListValue* printer_list = NULL;
      response_dict->GetList(kPrinterListValue, &printer_list);
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
      MessageLoop::current()->DeleteSoon(FROM_HERE, request_.release());
      if (!printer_list_.empty()) {
        // Let the frontend know that we have a list of printers available.
        backend_->frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
            &Core::NotifyPrinterListAvailable, printer_list_));
      } else {
        // No more work to be done here.
        MessageLoop::current()->PostTask(
            FROM_HERE, NewRunnableMethod(this, &Core::EndRegistration));
      }
    }
  }

  if (!succeeded) {
    HandleServerError(NewRunnableMethod(this, &Core::GetRegisteredPrinters));
  }
}

void CloudPrintProxyBackend::Core::InitJobHandlerForPrinter(
    DictionaryValue* printer_data) {
  DCHECK(printer_data);
  PrinterJobHandler::PrinterInfoFromCloud printer_info_cloud;
  printer_data->GetString(kIdValue, &printer_info_cloud.printer_id);
  DCHECK(!printer_info_cloud.printer_id.empty());
  LOG(INFO) << "CP_PROXY: Init job handler for printer id: " <<
      printer_info_cloud.printer_id;
  JobHandlerMap::iterator index = job_handler_map_.find(
      printer_info_cloud.printer_id);
  // We might already have a job handler for this printer
  if (index == job_handler_map_.end()) {
    cloud_print::PrinterBasicInfo printer_info;
    printer_data->GetString(kNameValue, &printer_info.printer_name);
    DCHECK(!printer_info.printer_name.empty());
    printer_data->GetString(kPrinterDescValue,
                            &printer_info.printer_description);
    printer_data->GetInteger(kPrinterStatusValue,
                             &printer_info.printer_status);
    printer_data->GetString(kPrinterCapsHashValue,
        &printer_info_cloud.caps_hash);
    ListValue* tags_list = NULL;
    printer_data->GetList(kPrinterTagsValue, &tags_list);
    if (tags_list) {
      for (size_t index = 0; index < tags_list->GetSize(); index++) {
        std::string tag;
        tags_list->GetString(index, &tag);
        if (StartsWithASCII(tag, kTagsHashTagName, false)) {
          std::vector<std::string> tag_parts;
          SplitStringDontTrim(tag, '=', &tag_parts);
          DCHECK(tag_parts.size() == 2);
          if (tag_parts.size() == 2) {
            printer_info_cloud.tags_hash = tag_parts[1];
          }
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

void CloudPrintProxyBackend::Core::HandleRegisterPrinterResponse(
    const URLFetcher* source, const GURL& url, const URLRequestStatus& status,
    int response_code, const ResponseCookies& cookies,
    const std::string& data) {
  LOG(INFO) << "CP_PROXY: Handle register printer response, code: " <<
      response_code;
  Task* next_task =
      NewRunnableMethod(this,
                        &CloudPrintProxyBackend::Core::RegisterNextPrinter);
  if (status.is_success() && (response_code == 200)) {
    bool succeeded = false;
    DictionaryValue* response_dict = NULL;
    CloudPrintHelpers::ParseResponseJSON(data, &succeeded, &response_dict);
    if (succeeded) {
      DCHECK(response_dict);
      ListValue* printer_list = NULL;
      response_dict->GetList(kPrinterListValue, &printer_list);
      // There should be a "printers" value in the JSON
      DCHECK(printer_list);
      if (printer_list) {
        DictionaryValue* printer_data = NULL;
        if (printer_list->GetDictionary(0, &printer_data)) {
          InitJobHandlerForPrinter(printer_data);
        }
      }
    }
    server_error_count_ = 0;
    next_upload_index_++;
    MessageLoop::current()->PostTask(FROM_HERE, next_task);
  } else {
    HandleServerError(next_task);
  }
}

void CloudPrintProxyBackend::Core::HandleServerError(Task* task_to_retry) {
  LOG(INFO) << "CP_PROXY: Server error.";
  CloudPrintHelpers::HandleServerError(
      &server_error_count_, -1, kMaxRetryInterval, kBaseRetryInterval,
      task_to_retry, NULL);
}

bool CloudPrintProxyBackend::Core::RemovePrinterFromList(
    const std::string& printer_name) {
  bool ret = false;
  for (cloud_print::PrinterList::iterator index = printer_list_.begin();
       index != printer_list_.end(); index++) {
    if (0 == base::strcasecmp(index->printer_name.c_str(),
                              printer_name.c_str())) {
      index = printer_list_.erase(index);
      ret = true;
      break;
    }
  }
  return ret;
}

void CloudPrintProxyBackend::Core::OnNotificationStateChange(
    bool notification_enabled) {}

void CloudPrintProxyBackend::Core::OnIncomingNotification(
    const IncomingNotificationData& notification_data) {
  LOG(INFO) << "CP_PROXY: Incoming notification.";
  if (0 == base::strcasecmp(kCloudPrintTalkServiceUrl,
                             notification_data.service_url.c_str())) {
    backend_->core_thread_.message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(
            this, &CloudPrintProxyBackend::Core::DoHandlePrinterNotification,
            notification_data.service_specific_data));
  }
}

void CloudPrintProxyBackend::Core::OnOutgoingNotification() {}

// cloud_print::PrinterChangeNotifier::Delegate implementation
void CloudPrintProxyBackend::Core::OnPrinterAdded() {
  if (request_.get()) {
    new_printers_available_ = true;
  } else {
    StartRegistration();
  }
}

// PrinterJobHandler::Delegate implementation
void CloudPrintProxyBackend::Core::OnPrinterJobHandlerShutdown(
    PrinterJobHandler* job_handler, const std::string& printer_id) {
  LOG(INFO) << "CP_PROXY: Printer job handle shutdown, id " << printer_id;
  job_handler_map_.erase(printer_id);
}
