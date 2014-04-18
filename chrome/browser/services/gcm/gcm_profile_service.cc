// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/gcm_profile_service.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/services/gcm/gcm_app_handler.h"
#include "chrome/browser/services/gcm/gcm_client_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "google_apis/gcm/protocol/android_checkin.pb.h"
#include "net/url_request/url_request_context_getter.h"

namespace gcm {

namespace {

checkin_proto::ChromeBuildProto_Platform GetPlatform() {
#if defined(OS_WIN)
  return checkin_proto::ChromeBuildProto_Platform_PLATFORM_WIN;
#elif defined(OS_MACOSX)
  return checkin_proto::ChromeBuildProto_Platform_PLATFORM_MAC;
#elif defined(OS_IOS)
  return checkin_proto::ChromeBuildProto_Platform_PLATFORM_IOS;
#elif defined(OS_CHROMEOS)
  return checkin_proto::ChromeBuildProto_Platform_PLATFORM_CROS;
#elif defined(OS_LINUX)
  return checkin_proto::ChromeBuildProto_Platform_PLATFORM_LINUX;
#else
  // For all other platforms, return as LINUX.
  return checkin_proto::ChromeBuildProto_Platform_PLATFORM_LINUX;
#endif
}

std::string GetVersion() {
  chrome::VersionInfo version_info;
  return version_info.Version();
}

checkin_proto::ChromeBuildProto_Channel GetChannel() {
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  switch (channel) {
    case chrome::VersionInfo::CHANNEL_UNKNOWN:
      return checkin_proto::ChromeBuildProto_Channel_CHANNEL_UNKNOWN;
    case chrome::VersionInfo::CHANNEL_CANARY:
      return checkin_proto::ChromeBuildProto_Channel_CHANNEL_CANARY;
    case chrome::VersionInfo::CHANNEL_DEV:
      return checkin_proto::ChromeBuildProto_Channel_CHANNEL_DEV;
    case chrome::VersionInfo::CHANNEL_BETA:
      return checkin_proto::ChromeBuildProto_Channel_CHANNEL_BETA;
    case chrome::VersionInfo::CHANNEL_STABLE:
      return checkin_proto::ChromeBuildProto_Channel_CHANNEL_STABLE;
    default:
      NOTREACHED();
      return checkin_proto::ChromeBuildProto_Channel_CHANNEL_UNKNOWN;
  };
}

}  // namespace

// Helper class to save tasks to run until we're ready to execute them.
class GCMProfileService::DelayedTaskController {
 public:
  DelayedTaskController();
  ~DelayedTaskController();

  // Adds a task that will be invoked once we're ready.
  void AddTask(base::Closure task);

  // Sets ready status. It is ready only when check-in is completed and
  // the GCMClient is fully initialized.
  void SetReady();

  // Returns true if it is ready to perform tasks.
  bool CanRunTaskWithoutDelay() const;

 private:
  void RunTasks();

  // Flag that indicates that GCM is ready.
  bool ready_;

  std::vector<base::Closure> delayed_tasks_;
};

GCMProfileService::DelayedTaskController::DelayedTaskController()
    : ready_(false) {
}

GCMProfileService::DelayedTaskController::~DelayedTaskController() {
}

void GCMProfileService::DelayedTaskController::AddTask(base::Closure task) {
  delayed_tasks_.push_back(task);
}

void GCMProfileService::DelayedTaskController::SetReady() {
  ready_ = true;
  RunTasks();
}

bool GCMProfileService::DelayedTaskController::CanRunTaskWithoutDelay() const {
  return ready_;
}

void GCMProfileService::DelayedTaskController::RunTasks() {
  DCHECK(ready_);

  for (size_t i = 0; i < delayed_tasks_.size(); ++i)
    delayed_tasks_[i].Run();
  delayed_tasks_.clear();
}

class GCMProfileService::IOWorker
    : public GCMClient::Delegate,
      public base::RefCountedThreadSafe<GCMProfileService::IOWorker>{
 public:
  // Called on UI thread.
  IOWorker();

  // Overridden from GCMClient::Delegate:
  // Called on IO thread.
  virtual void OnRegisterFinished(const std::string& app_id,
                                  const std::string& registration_id,
                                  GCMClient::Result result) OVERRIDE;
  virtual void OnUnregisterFinished(const std::string& app_id,
                                    GCMClient::Result result) OVERRIDE;
  virtual void OnSendFinished(const std::string& app_id,
                              const std::string& message_id,
                              GCMClient::Result result) OVERRIDE;
  virtual void OnMessageReceived(
      const std::string& app_id,
      const GCMClient::IncomingMessage& message) OVERRIDE;
  virtual void OnMessagesDeleted(const std::string& app_id) OVERRIDE;
  virtual void OnMessageSendError(
      const std::string& app_id,
      const GCMClient::SendErrorDetails& send_error_details) OVERRIDE;
  virtual void OnGCMReady() OVERRIDE;

  // Called on IO thread.
  void Initialize(scoped_ptr<GCMClientFactory> gcm_client_factory,
                  const base::FilePath& store_path,
                  const std::vector<std::string>& account_ids,
                  const scoped_refptr<net::URLRequestContextGetter>&
                      url_request_context_getter);
  void Reset();
  void Load(const base::WeakPtr<GCMProfileService>& service);
  void Stop();
  void CheckOut();
  void Register(const std::string& app_id,
                const std::vector<std::string>& sender_ids);
  void Unregister(const std::string& app_id);
  void Send(const std::string& app_id,
            const std::string& receiver_id,
            const GCMClient::OutgoingMessage& message);
  void GetGCMStatistics(bool clear_logs);
  void SetGCMRecording(bool recording);

  // For testing purpose. Can be called from UI thread. Use with care.
  GCMClient* gcm_client_for_testing() const { return gcm_client_.get(); }

 private:
  friend class base::RefCountedThreadSafe<IOWorker>;
  virtual ~IOWorker();

  base::WeakPtr<GCMProfileService> service_;

  scoped_ptr<GCMClient> gcm_client_;
};

GCMProfileService::IOWorker::IOWorker() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

GCMProfileService::IOWorker::~IOWorker() {
}

void GCMProfileService::IOWorker::Initialize(
    scoped_ptr<GCMClientFactory> gcm_client_factory,
    const base::FilePath& store_path,
    const std::vector<std::string>& account_ids,
    const scoped_refptr<net::URLRequestContextGetter>&
        url_request_context_getter) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  gcm_client_ = gcm_client_factory->BuildInstance().Pass();

  checkin_proto::ChromeBuildProto chrome_build_proto;
  chrome_build_proto.set_platform(GetPlatform());
  chrome_build_proto.set_chrome_version(GetVersion());
  chrome_build_proto.set_channel(GetChannel());

  scoped_refptr<base::SequencedWorkerPool> worker_pool(
      content::BrowserThread::GetBlockingPool());
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner(
      worker_pool->GetSequencedTaskRunnerWithShutdownBehavior(
          worker_pool->GetSequenceToken(),
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));

  gcm_client_->Initialize(chrome_build_proto,
                          store_path,
                          account_ids,
                          blocking_task_runner,
                          url_request_context_getter,
                          this);
}

void GCMProfileService::IOWorker::Reset() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  // GCMClient instance must be destroyed from the same thread where it was
  // created.
  gcm_client_.reset();
}

void GCMProfileService::IOWorker::OnRegisterFinished(
    const std::string& app_id,
    const std::string& registration_id,
    GCMClient::Result result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GCMProfileService::RegisterFinished,
                 service_,
                 app_id,
                 registration_id,
                 result));
}

void GCMProfileService::IOWorker::OnUnregisterFinished(
    const std::string& app_id,
    GCMClient::Result result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &GCMProfileService::UnregisterFinished, service_, app_id, result));
}

void GCMProfileService::IOWorker::OnSendFinished(
    const std::string& app_id,
    const std::string& message_id,
    GCMClient::Result result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GCMProfileService::SendFinished,
                 service_,
                 app_id,
                 message_id,
                 result));
}

void GCMProfileService::IOWorker::OnMessageReceived(
    const std::string& app_id,
    const GCMClient::IncomingMessage& message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GCMProfileService::MessageReceived,
                 service_,
                 app_id,
                 message));
}

void GCMProfileService::IOWorker::OnMessagesDeleted(const std::string& app_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GCMProfileService::MessagesDeleted,
                 service_,
                 app_id));
}

void GCMProfileService::IOWorker::OnMessageSendError(
    const std::string& app_id,
    const GCMClient::SendErrorDetails& send_error_details) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GCMProfileService::MessageSendError,
                 service_,
                 app_id,
                 send_error_details));
}

void GCMProfileService::IOWorker::OnGCMReady() {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GCMProfileService::GCMClientReady,
                 service_));
}

void GCMProfileService::IOWorker::Load(
    const base::WeakPtr<GCMProfileService>& service) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  service_ = service;
  gcm_client_->Load();
}

void GCMProfileService::IOWorker::Stop() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  gcm_client_->Stop();
}

void GCMProfileService::IOWorker::CheckOut() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  gcm_client_->CheckOut();

  // Note that we still need to keep GCMClient instance alive since the profile
  // might be signed in again.
}

void GCMProfileService::IOWorker::Register(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  gcm_client_->Register(app_id, sender_ids);
}

void GCMProfileService::IOWorker::Unregister(const std::string& app_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  gcm_client_->Unregister(app_id);
}

void GCMProfileService::IOWorker::Send(
    const std::string& app_id,
    const std::string& receiver_id,
    const GCMClient::OutgoingMessage& message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  gcm_client_->Send(app_id, receiver_id, message);
}

void GCMProfileService::IOWorker::GetGCMStatistics(bool clear_logs) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  gcm::GCMClient::GCMStatistics stats;

  if (gcm_client_.get()) {
    if (clear_logs)
      gcm_client_->ClearActivityLogs();
    stats = gcm_client_->GetStatistics();
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GCMProfileService::GetGCMStatisticsFinished,
                 service_,
                 stats));
}

void GCMProfileService::IOWorker::SetGCMRecording(bool recording) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  gcm::GCMClient::GCMStatistics stats;

  if (gcm_client_.get()) {
    gcm_client_->SetRecording(recording);
    stats = gcm_client_->GetStatistics();
    stats.gcm_client_created = true;
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GCMProfileService::GetGCMStatisticsFinished,
                 service_,
                 stats));
}

std::string GCMProfileService::GetGCMEnabledStateString(GCMEnabledState state) {
  switch (state) {
    case GCMProfileService::ALWAYS_ENABLED:
      return "ALWAYS_ENABLED";
    case GCMProfileService::ENABLED_FOR_APPS:
      return "ENABLED_FOR_APPS";
    case GCMProfileService::ALWAYS_DISABLED:
      return "ALWAYS_DISABLED";
    default:
      NOTREACHED();
      return std::string();
  }
}

// static
GCMProfileService::GCMEnabledState GCMProfileService::GetGCMEnabledState(
    Profile* profile) {
  const base::Value* gcm_enabled_value =
      profile->GetPrefs()->GetUserPrefValue(prefs::kGCMChannelEnabled);
  if (!gcm_enabled_value)
    return ENABLED_FOR_APPS;

  bool gcm_enabled = false;
  if (!gcm_enabled_value->GetAsBoolean(&gcm_enabled))
    return ENABLED_FOR_APPS;

  return gcm_enabled ? ALWAYS_ENABLED : ALWAYS_DISABLED;
}

// static
void GCMProfileService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // GCM support is only enabled by default for Canary/Dev/Custom builds.
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  bool on_by_default = false;
  if (channel == chrome::VersionInfo::CHANNEL_UNKNOWN ||
      channel == chrome::VersionInfo::CHANNEL_CANARY ||
      channel == chrome::VersionInfo::CHANNEL_DEV) {
    on_by_default = true;
  }
  registry->RegisterBooleanPref(
      prefs::kGCMChannelEnabled,
      on_by_default,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

GCMProfileService::GCMProfileService(Profile* profile)
    : profile_(profile),
      gcm_client_ready_(false),
      weak_ptr_factory_(this) {
  DCHECK(!profile->IsOffTheRecord());
}

GCMProfileService::~GCMProfileService() {
}

void GCMProfileService::Initialize(
    scoped_ptr<GCMClientFactory> gcm_client_factory) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::Source<Profile>(profile_));

  SigninManagerFactory::GetForProfile(profile_)->AddObserver(this);

  // Get the list of available accounts.
  std::vector<std::string> account_ids;
#if !defined(OS_ANDROID)
  account_ids =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)->GetAccounts();
#endif

  // Create and initialize the GCMClient. Note that this does not initiate the
  // GCM check-in.
  io_worker_ = new IOWorker();
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter =
      profile_->GetRequestContext();
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMProfileService::IOWorker::Initialize,
                 io_worker_,
                 base::Passed(&gcm_client_factory),
                 profile_->GetPath().Append(chrome::kGCMStoreDirname),
                 account_ids,
                 url_request_context_getter));

  // Load from the GCM store and initiate the GCM check-in if the rollout signal
  // indicates yes.
  if (GetGCMEnabledState(profile_) == ALWAYS_ENABLED)
    EnsureLoaded();
}

void GCMProfileService::Start() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  EnsureLoaded();
}

void GCMProfileService::Stop() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // No need to stop GCM service if not started yet.
  if (username_.empty())
    return;

  RemoveCachedData();

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMProfileService::IOWorker::Stop, io_worker_));
}

void GCMProfileService::Shutdown() {
  for (GCMAppHandlerMap::const_iterator iter = app_handlers_.begin();
       iter != app_handlers_.end(); ++iter) {
    iter->second->ShutdownHandler();
  }
  app_handlers_.clear();

  SigninManagerFactory::GetForProfile(profile_)->RemoveObserver(this);
}

void GCMProfileService::AddAppHandler(const std::string& app_id,
                                      GCMAppHandler* handler) {
  DCHECK(!app_id.empty());
  DCHECK(handler);
  DCHECK(app_handlers_.find(app_id) == app_handlers_.end());

  app_handlers_[app_id] = handler;
}

void GCMProfileService::RemoveAppHandler(const std::string& app_id) {
  DCHECK(!app_id.empty());

  app_handlers_.erase(app_id);
}

void GCMProfileService::Register(const std::string& app_id,
                                 const std::vector<std::string>& sender_ids,
                                 RegisterCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!app_id.empty() && !sender_ids.empty() && !callback.is_null());

  GCMClient::Result result = EnsureAppReady(app_id);
  if (result != GCMClient::SUCCESS) {
    callback.Run(std::string(), result);
    return;
  }

  // If previous un/register operation is still in progress, bail out.
  if (IsAsyncOperationPending(app_id)) {
    callback.Run(std::string(), GCMClient::ASYNC_OPERATION_PENDING);
    return;
  }

  register_callbacks_[app_id] = callback;

  // Delay the register operation until GCMClient is ready.
  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    delayed_task_controller_->AddTask(
        base::Bind(&GCMProfileService::DoRegister,
                   weak_ptr_factory_.GetWeakPtr(),
                   app_id,
                   sender_ids));
    return;
  }

  DoRegister(app_id, sender_ids);
}

void GCMProfileService::DoRegister(const std::string& app_id,
                                   const std::vector<std::string>& sender_ids) {
  std::map<std::string, RegisterCallback>::iterator callback_iter =
      register_callbacks_.find(app_id);
  if (callback_iter == register_callbacks_.end()) {
    // The callback could have been removed when the app is uninstalled.
    return;
  }

  // Normalize the sender IDs by making them sorted.
  std::vector<std::string> normalized_sender_ids = sender_ids;
  std::sort(normalized_sender_ids.begin(), normalized_sender_ids.end());

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMProfileService::IOWorker::Register,
                 io_worker_,
                 app_id,
                 normalized_sender_ids));
}

void GCMProfileService::Unregister(const std::string& app_id,
                                   UnregisterCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!app_id.empty() && !callback.is_null());

  GCMClient::Result result = EnsureAppReady(app_id);
  if (result != GCMClient::SUCCESS) {
    callback.Run(result);
    return;
  }

  // If previous un/register operation is still in progress, bail out.
  if (IsAsyncOperationPending(app_id)) {
    callback.Run(GCMClient::ASYNC_OPERATION_PENDING);
    return;
  }

  unregister_callbacks_[app_id] = callback;

  // Delay the unregister operation until GCMClient is ready.
  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    delayed_task_controller_->AddTask(
        base::Bind(&GCMProfileService::DoUnregister,
                   weak_ptr_factory_.GetWeakPtr(),
                   app_id));
    return;
  }

  DoUnregister(app_id);
}

void GCMProfileService::DoUnregister(const std::string& app_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Ask the server to unregister it. There could be a small chance that the
  // unregister request fails. If this occurs, it does not bring any harm since
  // we simply reject the messages/events received from the server.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMProfileService::IOWorker::Unregister,
                 io_worker_,
                 app_id));
}

void GCMProfileService::Send(const std::string& app_id,
                             const std::string& receiver_id,
                             const GCMClient::OutgoingMessage& message,
                             SendCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!app_id.empty() && !receiver_id.empty() && !callback.is_null());

  GCMClient::Result result = EnsureAppReady(app_id);
  if (result != GCMClient::SUCCESS) {
    callback.Run(std::string(), result);
    return;
  }

  // If the message with send ID is still in progress, bail out.
  std::pair<std::string, std::string> key(app_id, message.id);
  if (send_callbacks_.find(key) != send_callbacks_.end()) {
    callback.Run(message.id, GCMClient::INVALID_PARAMETER);
    return;
  }

  send_callbacks_[key] = callback;

  // Delay the send operation until all GCMClient is ready.
  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    delayed_task_controller_->AddTask(
        base::Bind(&GCMProfileService::DoSend,
                   weak_ptr_factory_.GetWeakPtr(),
                   app_id,
                   receiver_id,
                   message));
    return;
  }

  DoSend(app_id, receiver_id, message);
}

void GCMProfileService::DoSend(const std::string& app_id,
                               const std::string& receiver_id,
                               const GCMClient::OutgoingMessage& message) {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMProfileService::IOWorker::Send,
                 io_worker_,
                 app_id,
                 receiver_id,
                 message));
}

GCMClient* GCMProfileService::GetGCMClientForTesting() const {
  return io_worker_ ? io_worker_->gcm_client_for_testing() : NULL;
}

std::string GCMProfileService::SignedInUserName() const {
  return username_;
}

bool GCMProfileService::IsGCMClientReady() const {
  return gcm_client_ready_;
}

void GCMProfileService::GetGCMStatistics(
    GetGCMStatisticsCallback callback, bool clear_logs) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!callback.is_null());

  request_gcm_statistics_callback_ = callback;
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMProfileService::IOWorker::GetGCMStatistics,
                 io_worker_,
                 clear_logs));
}

void GCMProfileService::SetGCMRecording(
    GetGCMStatisticsCallback callback, bool recording) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  request_gcm_statistics_callback_ = callback;
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMProfileService::IOWorker::SetGCMRecording,
                 io_worker_,
                 recording));
}

void GCMProfileService::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  switch (type) {
    case chrome::NOTIFICATION_PROFILE_DESTROYED:
      ResetGCMClient();
      break;
    default:
      NOTREACHED();
  }
}

void GCMProfileService::GoogleSigninSucceeded(const std::string& username,
                                              const std::string& password) {
  if (GetGCMEnabledState(profile_) == ALWAYS_ENABLED)
    EnsureLoaded();
}

void GCMProfileService::GoogleSignedOut(const std::string& username) {
  CheckOut();
}

void GCMProfileService::EnsureLoaded() {
  SigninManagerBase* manager = SigninManagerFactory::GetForProfile(profile_);
  if (!manager)
    return;
  std::string username = manager->GetAuthenticatedUsername();
  if (username.empty())
    return;

  // CheckIn could be called more than once when:
  // 1) The password changes.
  // 2) Register/send function calls it to ensure CheckIn is done.
  if (username_ == username)
    return;
  username_ = username;

  DCHECK(!delayed_task_controller_);
  delayed_task_controller_.reset(new DelayedTaskController);

  // This will load the data from the gcm store and trigger the check-in if
  // the persisted check-in info is not found.
  // Note that we need to pass weak pointer again since the existing weak
  // pointer in IOWorker might have been invalidated when check-out occurs.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMProfileService::IOWorker::Load,
                 io_worker_,
                 weak_ptr_factory_.GetWeakPtr()));
}

void GCMProfileService::RemoveCachedData() {
  // Remove all the queued tasks since they no longer make sense after
  // GCM service is stopped.
  weak_ptr_factory_.InvalidateWeakPtrs();

  username_.clear();
  gcm_client_ready_ = false;
  delayed_task_controller_.reset();
  register_callbacks_.clear();
  send_callbacks_.clear();
}

void GCMProfileService::CheckOut() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // We still proceed with the check-out logic even if the check-in is not
  // initiated in the current session. This will make sure that all the
  // persisted data written previously will get purged.

  RemoveCachedData();

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMProfileService::IOWorker::CheckOut, io_worker_));
}

void GCMProfileService::ResetGCMClient() {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMProfileService::IOWorker::Reset, io_worker_));
}

GCMClient::Result GCMProfileService::EnsureAppReady(const std::string& app_id) {
  // Ensure that check-in has been done.
  EnsureLoaded();

  // If the profile was not signed in, bail out.
  if (username_.empty())
    return GCMClient::NOT_SIGNED_IN;

  return GCMClient::SUCCESS;
}

bool GCMProfileService::IsAsyncOperationPending(
    const std::string& app_id) const {
  return register_callbacks_.find(app_id) != register_callbacks_.end() ||
         unregister_callbacks_.find(app_id) != unregister_callbacks_.end();
}

void GCMProfileService::RegisterFinished(const std::string& app_id,
                                         const std::string& registration_id,
                                         GCMClient::Result result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  std::map<std::string, RegisterCallback>::iterator callback_iter =
      register_callbacks_.find(app_id);
  if (callback_iter == register_callbacks_.end()) {
    // The callback could have been removed when the app is uninstalled.
    return;
  }

  RegisterCallback callback = callback_iter->second;
  register_callbacks_.erase(callback_iter);
  callback.Run(registration_id, result);
}

void GCMProfileService::UnregisterFinished(const std::string& app_id,
                                           GCMClient::Result result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  std::map<std::string, UnregisterCallback>::iterator callback_iter =
      unregister_callbacks_.find(app_id);
  if (callback_iter == unregister_callbacks_.end())
    return;

  UnregisterCallback callback = callback_iter->second;
  unregister_callbacks_.erase(callback_iter);
  callback.Run(result);
}

void GCMProfileService::SendFinished(const std::string& app_id,
                                     const std::string& message_id,
                                     GCMClient::Result result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  std::map<std::pair<std::string, std::string>, SendCallback>::iterator
      callback_iter = send_callbacks_.find(
          std::pair<std::string, std::string>(app_id, message_id));
  if (callback_iter == send_callbacks_.end()) {
    // The callback could have been removed when the app is uninstalled.
    return;
  }

  SendCallback callback = callback_iter->second;
  send_callbacks_.erase(callback_iter);
  callback.Run(message_id, result);
}

void GCMProfileService::MessageReceived(const std::string& app_id,
                                        GCMClient::IncomingMessage message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Drop the event if signed out.
  if (username_.empty())
    return;

  GetAppHandler(app_id)->OnMessage(app_id, message);
}

void GCMProfileService::MessagesDeleted(const std::string& app_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Drop the event if signed out.
  if (username_.empty())
    return;

  GetAppHandler(app_id)->OnMessagesDeleted(app_id);
}

void GCMProfileService::MessageSendError(
    const std::string& app_id,
    const GCMClient::SendErrorDetails& send_error_details) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Drop the event if signed out.
  if (username_.empty())
    return;

  GetAppHandler(app_id)->OnSendError(app_id, send_error_details);
}

void GCMProfileService::GCMClientReady() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (gcm_client_ready_)
    return;
  gcm_client_ready_ = true;

  delayed_task_controller_->SetReady();
}

GCMAppHandler* GCMProfileService::GetAppHandler(const std::string& app_id) {
  std::map<std::string, GCMAppHandler*>::const_iterator iter =
      app_handlers_.find(app_id);
  return iter == app_handlers_.end() ? &default_app_handler_ : iter->second;
}

void GCMProfileService::GetGCMStatisticsFinished(
    GCMClient::GCMStatistics stats) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  request_gcm_statistics_callback_.Run(stats);
}

}  // namespace gcm
