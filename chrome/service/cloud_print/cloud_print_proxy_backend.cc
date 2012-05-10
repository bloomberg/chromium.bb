// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_proxy_backend.h"

#include <map>
#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/rand_util.h"
#include "base/values.h"
#include "chrome/common/net/gaia/gaia_oauth_client.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/service/cloud_print/cloud_print_auth.h"
#include "chrome/service/cloud_print/cloud_print_connector.h"
#include "chrome/service/cloud_print/cloud_print_consts.h"
#include "chrome/service/cloud_print/cloud_print_helpers.h"
#include "chrome/service/cloud_print/cloud_print_token_store.h"
#include "chrome/service/gaia/service_gaia_authenticator.h"
#include "chrome/service/net/service_url_request_context.h"
#include "chrome/service/service_process.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/listener/mediator_thread_impl.h"
#include "jingle/notifier/listener/talk_mediator_impl.h"
#include "ui/base/l10n/l10n_util.h"

// The real guts of CloudPrintProxyBackend, to keep the public client API clean.
class CloudPrintProxyBackend::Core
    : public base::RefCountedThreadSafe<CloudPrintProxyBackend::Core>,
      public CloudPrintAuth::Client,
      public CloudPrintConnector::Client,
      public notifier::TalkMediator::Delegate {
 public:
  // It is OK for print_server_url to be empty. In this case system should
  // use system default (local) print server.
  Core(CloudPrintProxyBackend* backend,
       const std::string& proxy_id,
       const GURL& cloud_print_server_url,
       const DictionaryValue* print_system_settings,
       const gaia::OAuthClientInfo& oauth_client_info,
       bool enable_job_poll);

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
                            const std::string& proxy_id,
                            const std::string& last_robot_refresh_token,
                            const std::string& last_robot_email,
                            const std::string& last_user_email);

  void DoInitializeWithToken(const std::string& cloud_print_token,
                             const std::string& proxy_id);
  void DoInitializeWithRobotToken(const std::string& robot_oauth_refresh_token,
                                  const std::string& robot_email,
                                  const std::string& proxy_id);
  void DoInitializeWithRobotAuthCode(const std::string& robot_oauth_auth_code,
                                     const std::string& robot_email,
                                     const std::string& proxy_id);

  // Called on the CloudPrintProxyBackend core_thread_ to perform
  // shutdown.
  void DoShutdown();
  void DoRegisterSelectedPrinters(
      const printing::PrinterList& printer_list);
  void DoUnregisterPrinters();

  // CloudPrintAuth::Client implementation.
  virtual void OnAuthenticationComplete(
      const std::string& access_token,
      const std::string& robot_oauth_refresh_token,
      const std::string& robot_email,
      const std::string& user_email);
  virtual void OnInvalidCredentials();

  // CloudPrintConnector::Client implementation.
  virtual void OnAuthFailed();

  // notifier::TalkMediator::Delegate implementation.
  virtual void OnNotificationStateChange(
      bool notifications_enabled);
  virtual void OnIncomingNotification(
      const notifier::Notification& notification);
  virtual void OnOutgoingNotification();

 private:
  friend class base::RefCountedThreadSafe<Core>;

  virtual ~Core() {}

  void CreateAuthAndConnector();
  void DestroyAuthAndConnector();

  // NotifyXXX is how the Core communicates with the frontend across
  // threads.
  void NotifyPrinterListAvailable(
      const printing::PrinterList& printer_list);
  void NotifyAuthenticated(
    const std::string& robot_oauth_refresh_token,
    const std::string& robot_email,
    const std::string& user_email);
  void NotifyAuthenticationFailed();
  void NotifyPrintSystemUnavailable();
  void NotifyUnregisterPrinters(const std::string& auth_token,
                                const std::list<std::string>& printer_ids);

  // Init XMPP channel
  void InitNotifications(const std::string& robot_email,
                         const std::string& access_token);

  void HandlePrinterNotification(const std::string& printer_id);
  void PollForJobs();
  // Schedules a task to poll for jobs. Does nothing if a task is already
  // scheduled.
  void ScheduleJobPoll();
  CloudPrintTokenStore* GetTokenStore();

  // Our parent CloudPrintProxyBackend
  CloudPrintProxyBackend* backend_;

  // Cloud Print authenticator.
  scoped_refptr<CloudPrintAuth> auth_;

  // Cloud Print connector.
  scoped_refptr<CloudPrintConnector> connector_;

  // Server URL.
  GURL cloud_print_server_url_;
  // Proxy Id.
  std::string proxy_id_;
  // Print system settings.
  scoped_ptr<DictionaryValue> print_system_settings_;
  // OAuth client info.
  gaia::OAuthClientInfo oauth_client_info_;
  // Notification (xmpp) handler.
  scoped_ptr<notifier::TalkMediator> talk_mediator_;
  // Indicates whether XMPP notifications are currently enabled.
  bool notifications_enabled_;
  // The time when notifications were enabled. Valid only when
  // notifications_enabled_ is true.
  base::TimeTicks notifications_enabled_since_;
  // Indicates whether a task to poll for jobs has been scheduled.
  bool job_poll_scheduled_;
  // Indicates whether we should poll for jobs when we lose XMPP connection.
  bool enable_job_poll_;
  scoped_ptr<CloudPrintTokenStore> token_store_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

CloudPrintProxyBackend::CloudPrintProxyBackend(
    CloudPrintProxyFrontend* frontend,
    const std::string& proxy_id,
    const GURL& cloud_print_server_url,
    const DictionaryValue* print_system_settings,
    const gaia::OAuthClientInfo& oauth_client_info,
    bool enable_job_poll)
      : core_thread_("Chrome_CloudPrintProxyCoreThread"),
        frontend_loop_(MessageLoop::current()),
        frontend_(frontend) {
  DCHECK(frontend_);
  core_ = new Core(this,
                   proxy_id,
                   cloud_print_server_url,
                   print_system_settings,
                   oauth_client_info,
                   enable_job_poll);
}

CloudPrintProxyBackend::~CloudPrintProxyBackend() {
  DCHECK(!core_);
}

bool CloudPrintProxyBackend::InitializeWithLsid(
    const std::string& lsid,
    const std::string& proxy_id,
    const std::string& last_robot_refresh_token,
    const std::string& last_robot_email,
    const std::string& last_user_email) {
  if (!core_thread_.Start())
    return false;
  core_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&CloudPrintProxyBackend::Core::DoInitializeWithLsid,
                 core_.get(), lsid, proxy_id, last_robot_refresh_token,
                 last_robot_email, last_user_email));
  return true;
}

bool CloudPrintProxyBackend::InitializeWithToken(
    const std::string& cloud_print_token,
    const std::string& proxy_id) {
  if (!core_thread_.Start())
    return false;
  core_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&CloudPrintProxyBackend::Core::DoInitializeWithToken,
                 core_.get(), cloud_print_token, proxy_id));
  return true;
}

bool CloudPrintProxyBackend::InitializeWithRobotToken(
    const std::string& robot_oauth_refresh_token,
    const std::string& robot_email,
    const std::string& proxy_id) {
  if (!core_thread_.Start())
    return false;
  core_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&CloudPrintProxyBackend::Core::DoInitializeWithRobotToken,
                 core_.get(), robot_oauth_refresh_token, robot_email,
                 proxy_id));
  return true;
}

bool CloudPrintProxyBackend::InitializeWithRobotAuthCode(
    const std::string& robot_oauth_auth_code,
    const std::string& robot_email,
    const std::string& proxy_id) {
  if (!core_thread_.Start())
    return false;
  core_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&CloudPrintProxyBackend::Core::DoInitializeWithRobotAuthCode,
                 core_.get(), robot_oauth_auth_code, robot_email, proxy_id));
  return true;
}

void CloudPrintProxyBackend::Shutdown() {
  core_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&CloudPrintProxyBackend::Core::DoShutdown, core_.get()));
  core_thread_.Stop();
  core_ = NULL;  // Releases reference to core_.
}

void CloudPrintProxyBackend::UnregisterPrinters() {
  core_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&CloudPrintProxyBackend::Core::DoUnregisterPrinters,
                 core_.get()));
}

CloudPrintProxyBackend::Core::Core(
    CloudPrintProxyBackend* backend,
    const std::string& proxy_id,
    const GURL& cloud_print_server_url,
    const DictionaryValue* print_system_settings,
    const gaia::OAuthClientInfo& oauth_client_info,
    bool enable_job_poll)
      : backend_(backend),
        cloud_print_server_url_(cloud_print_server_url),
        proxy_id_(proxy_id),
        oauth_client_info_(oauth_client_info),
        notifications_enabled_(false),
        job_poll_scheduled_(false),
        enable_job_poll_(enable_job_poll) {
  if (print_system_settings) {
    // It is possible to have no print settings specified.
    print_system_settings_.reset(print_system_settings->DeepCopy());
  }
}

void CloudPrintProxyBackend::Core::CreateAuthAndConnector() {
  if (!auth_.get()) {
    auth_ = new CloudPrintAuth(this,
                               cloud_print_server_url_,
                               print_system_settings_.get(),
                               oauth_client_info_,
                               proxy_id_);
  }

  if (!connector_.get()) {
    connector_ = new CloudPrintConnector(this,
                                         proxy_id_,
                                         cloud_print_server_url_,
                                         print_system_settings_.get());
  }
}

void CloudPrintProxyBackend::Core::DestroyAuthAndConnector() {
  auth_ = NULL;
  connector_ = NULL;
}

void CloudPrintProxyBackend::Core::DoInitializeWithLsid(
    const std::string& lsid,
    const std::string& proxy_id,
    const std::string& last_robot_refresh_token,
    const std::string& last_robot_email,
    const std::string& last_user_email) {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  CreateAuthAndConnector();
  // Note: The GAIA login is synchronous but that should be OK because we are in
  // the CloudPrintProxyCoreThread and we cannot really do anything else until
  // the GAIA signin is successful.
  auth_->AuthenticateWithLsid(lsid, last_robot_refresh_token,
                              last_robot_email, last_user_email);
}

void CloudPrintProxyBackend::Core::DoInitializeWithToken(
    const std::string& cloud_print_token,
    const std::string& proxy_id) {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  CreateAuthAndConnector();
  auth_->AuthenticateWithToken(cloud_print_token);
}

void CloudPrintProxyBackend::Core::DoInitializeWithRobotToken(
    const std::string& robot_oauth_refresh_token,
    const std::string& robot_email,
    const std::string& proxy_id) {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  CreateAuthAndConnector();
  auth_->AuthenticateWithRobotToken(robot_oauth_refresh_token, robot_email);
}

void CloudPrintProxyBackend::Core::DoInitializeWithRobotAuthCode(
    const std::string& robot_oauth_auth_code,
    const std::string& robot_email,
    const std::string& proxy_id) {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  CreateAuthAndConnector();
  auth_->AuthenticateWithRobotAuthCode(robot_oauth_auth_code, robot_email);
}

void CloudPrintProxyBackend::Core::OnAuthenticationComplete(
    const std::string& access_token,
    const std::string& robot_oauth_refresh_token,
    const std::string& robot_email,
    const std::string& user_email) {
  CloudPrintTokenStore* token_store = GetTokenStore();
  bool first_time = token_store->token().empty();
  token_store->SetToken(access_token);
  // Let the frontend know that we have authenticated.
  backend_->frontend_loop_->PostTask(
      FROM_HERE,
      base::Bind(&Core::NotifyAuthenticated, this, robot_oauth_refresh_token,
                 robot_email, user_email));
  if (first_time) {
    InitNotifications(robot_email, access_token);
  } else {
    // If we are refreshing a token, update the XMPP token too.
    DCHECK(talk_mediator_.get());
    talk_mediator_->SetAuthToken(robot_email,
                                 access_token,
                                 kSyncGaiaServiceId);
  }
  // Start cloud print connector if needed.
  if (!connector_->IsRunning()) {
    if (!connector_->Start()) {
      // Let the frontend know that we do not have a print system.
      backend_->frontend_loop_->PostTask(
          FROM_HERE, base::Bind(&Core::NotifyPrintSystemUnavailable, this));
    }
  }
}

void CloudPrintProxyBackend::Core::OnInvalidCredentials() {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  VLOG(1) << "CP_CONNECTOR: Auth Error";
  backend_->frontend_loop_->PostTask(
      FROM_HERE, base::Bind(&Core::NotifyAuthenticationFailed, this));
}

void CloudPrintProxyBackend::Core::OnAuthFailed() {
  VLOG(1) << "CP_CONNECTOR: Authentication failed in connector.";
  // Let's stop connecter and refresh token. We'll restart connecter once
  // new token available.
  if (connector_->IsRunning())
    connector_->Stop();

  // Refresh Auth token.
  auth_->RefreshAccessToken();
}

void CloudPrintProxyBackend::Core::InitNotifications(
    const std::string& robot_email,
    const std::string& access_token) {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());

  notifier::NotifierOptions notifier_options;
  notifier_options.request_context_getter =
      g_service_process->GetServiceURLRequestContextGetter();
  notifier_options.auth_mechanism = "X-OAUTH2";
  talk_mediator_.reset(new notifier::TalkMediatorImpl(
      new notifier::MediatorThreadImpl(notifier_options),
      notifier_options));
  notifier::Subscription subscription;
  subscription.channel = kCloudPrintPushNotificationsSource;
  subscription.from = kCloudPrintPushNotificationsSource;
  talk_mediator_->AddSubscription(subscription);
  talk_mediator_->SetDelegate(this);
  talk_mediator_->SetAuthToken(robot_email, access_token, kSyncGaiaServiceId);
  talk_mediator_->Login();
}

void CloudPrintProxyBackend::Core::DoShutdown() {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  VLOG(1) << "CP_CONNECTOR: Shutdown connector, id: " << proxy_id_;

  if (connector_->IsRunning())
    connector_->Stop();

  // Important to delete the TalkMediator on this thread.
  if (talk_mediator_.get())
    talk_mediator_->Logout();
  talk_mediator_.reset();
  notifications_enabled_ = false;
  notifications_enabled_since_ = base::TimeTicks();
  token_store_.reset();

  DestroyAuthAndConnector();
}

void CloudPrintProxyBackend::Core::DoUnregisterPrinters() {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());

  std::string access_token = GetTokenStore()->token();

  std::list<std::string> printer_ids;
  connector_->GetPrinterIds(&printer_ids);

  backend_->frontend_loop_->PostTask(
      FROM_HERE,
      base::Bind(&Core::NotifyUnregisterPrinters,
                 this, access_token, printer_ids));
}

void CloudPrintProxyBackend::Core::HandlePrinterNotification(
    const std::string& printer_id) {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  VLOG(1) << "CP_CONNECTOR: Handle printer notification, id: " << printer_id;
  connector_->CheckForJobs(kJobFetchReasonNotified, printer_id);
}

void CloudPrintProxyBackend::Core::PollForJobs() {
  VLOG(1) << "CP_CONNECTOR: Polling for jobs.";
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  // Check all printers for jobs.
  connector_->CheckForJobs(kJobFetchReasonPoll, std::string());

  job_poll_scheduled_ = false;
  // If we don't have notifications and job polling is enabled, poll again
  // after a while.
  if (!notifications_enabled_ && enable_job_poll_)
    ScheduleJobPoll();
}

void CloudPrintProxyBackend::Core::ScheduleJobPoll() {
  if (!job_poll_scheduled_) {
    base::TimeDelta interval = base::TimeDelta::FromSeconds(
        base::RandInt(kMinJobPollIntervalSecs, kMaxJobPollIntervalSecs));
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CloudPrintProxyBackend::Core::PollForJobs, this),
        interval);
    job_poll_scheduled_ = true;
  }
}

CloudPrintTokenStore* CloudPrintProxyBackend::Core::GetTokenStore() {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  if (!token_store_.get())
    token_store_.reset(new CloudPrintTokenStore);
  return token_store_.get();
}

void CloudPrintProxyBackend::Core::NotifyAuthenticated(
    const std::string& robot_oauth_refresh_token,
    const std::string& robot_email,
    const std::string& user_email) {
  DCHECK(MessageLoop::current() == backend_->frontend_loop_);
  backend_->frontend_->OnAuthenticated(robot_oauth_refresh_token,
                                       robot_email,
                                       user_email);
}

void CloudPrintProxyBackend::Core::NotifyAuthenticationFailed() {
  DCHECK(MessageLoop::current() == backend_->frontend_loop_);
  backend_->frontend_->OnAuthenticationFailed();
}

void CloudPrintProxyBackend::Core::NotifyPrintSystemUnavailable() {
  DCHECK(MessageLoop::current() == backend_->frontend_loop_);
  backend_->frontend_->OnPrintSystemUnavailable();
}

void CloudPrintProxyBackend::Core::NotifyUnregisterPrinters(
    const std::string& auth_token,
    const std::list<std::string>& printer_ids) {
  DCHECK(MessageLoop::current() == backend_->frontend_loop_);
  backend_->frontend_->OnUnregisterPrinters(auth_token, printer_ids);
}

void CloudPrintProxyBackend::Core::OnNotificationStateChange(
    bool notification_enabled) {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  notifications_enabled_ = notification_enabled;
  if (notifications_enabled_) {
    notifications_enabled_since_ = base::TimeTicks::Now();
    VLOG(1) << "Notifications for connector " << proxy_id_
            << " were enabled at "
            << notifications_enabled_since_.ToInternalValue();
  } else {
    LOG(ERROR) << "Notifications for connector " << proxy_id_ << " disabled.";
    notifications_enabled_since_ = base::TimeTicks();
  }
  // A state change means one of two cases.
  // Case 1: We just lost notifications. This this case we want to schedule a
  // job poll if enable_job_poll_ is true.
  // Case 2: Notifications just got re-enabled. In this case we want to schedule
  // a poll once for jobs we might have missed when we were dark.
  // Note that ScheduleJobPoll will not schedule again if a job poll task is
  // already scheduled.
  if (enable_job_poll_ || notifications_enabled_)
    ScheduleJobPoll();
}


void CloudPrintProxyBackend::Core::OnIncomingNotification(
    const notifier::Notification& notification) {
  DCHECK(MessageLoop::current() == backend_->core_thread_.message_loop());
  VLOG(1) << "CP_CONNECTOR: Incoming notification.";
  if (0 == base::strcasecmp(kCloudPrintPushNotificationsSource,
                            notification.channel.c_str()))
    HandlePrinterNotification(notification.data);
}

void CloudPrintProxyBackend::Core::OnOutgoingNotification() {}
