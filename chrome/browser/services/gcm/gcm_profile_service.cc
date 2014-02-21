// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/gcm_profile_service.h"

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chrome_notification_types.h"
#if !defined(OS_ANDROID)
#include "chrome/browser/extensions/api/gcm/gcm_api.h"
#endif
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/state_store.h"
#include "chrome/browser/services/gcm/gcm_client_factory.h"
#include "chrome/browser/services/gcm/gcm_event_router.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "google_apis/gcm/protocol/android_checkin.pb.h"
#include "net/url_request/url_request_context_getter.h"

using extensions::Extension;

namespace gcm {

namespace {

const char kRegistrationKey[] = "gcm.registration";
const char kSendersKey[] = "senders";
const char kRegistrationIDKey[] = "reg_id";

checkin_proto::ChromeBuildProto_Platform GetPlatform() {
#if defined(OS_WIN)
  return checkin_proto::ChromeBuildProto_Platform_PLATFORM_WIN;
#elif defined(OS_MACOSX)
  return checkin_proto::ChromeBuildProto_Platform_PLATFORM_MAC;
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

  // Adds an app to the tracking list. It will be first marked as not ready.
  // Tasks will be queued for delay execution until the app is marked as ready.
  void AddApp(const std::string& app_id);

  // Removes the app from the tracking list.
  void RemoveApp(const std::string& app_id);

  // Adds a task that will be invoked once we're ready.
  void AddTask(const std::string& app_id, base::Closure task);

  // Sets GCM ready status. GCM is ready only when check-in is completed and
  // the GCMClient is fully initialized. If it is set to ready for the first
  // time, all the pending tasks for any ready apps will be run.
  void SetGCMReady();

  // Sets ready status for the app. If GCM is already ready, all the pending
  // tasks for this app will be run.
  void SetAppReady(const std::string& app_id);

  // Returns true if it is ready to perform operations for an app.
  bool CanRunTaskWithoutDelay(const std::string& app_id) const;

  // Returns true if the app has been tracked for readiness.
  bool IsAppTracked(const std::string& app_id) const;

 private:
  struct AppTaskQueue {
    AppTaskQueue();
    ~AppTaskQueue();

    // The flag that indicates if GCMClient is ready.
    bool ready;

    // Tasks to be invoked upon ready.
    std::vector<base::Closure> tasks;
  };

  void RunTasks(AppTaskQueue* task_queue);

  // Flag that indicates that GCM is done.
  bool gcm_ready_;

  // Map from app_id to AppTaskQueue storing the tasks that will be invoked
  // when both GCM and the app get ready.
  typedef std::map<std::string, AppTaskQueue*> DelayedTaskMap;
  DelayedTaskMap delayed_task_map_;
};

GCMProfileService::DelayedTaskController::AppTaskQueue::AppTaskQueue()
    : ready(false) {
}

GCMProfileService::DelayedTaskController::AppTaskQueue::~AppTaskQueue() {
}

GCMProfileService::DelayedTaskController::DelayedTaskController()
    : gcm_ready_(false) {
}

GCMProfileService::DelayedTaskController::~DelayedTaskController() {
  for (DelayedTaskMap::const_iterator iter = delayed_task_map_.begin();
       iter != delayed_task_map_.end(); ++iter) {
    delete iter->second;
  }
}

void GCMProfileService::DelayedTaskController::AddApp(
    const std::string& app_id) {
  DCHECK(delayed_task_map_.find(app_id) == delayed_task_map_.end());
  delayed_task_map_[app_id] = new AppTaskQueue;
}

void GCMProfileService::DelayedTaskController::RemoveApp(
    const std::string& app_id) {
  DelayedTaskMap::iterator iter = delayed_task_map_.find(app_id);
  if (iter == delayed_task_map_.end())
    return;
  delete iter->second;
  delayed_task_map_.erase(iter);
}

void GCMProfileService::DelayedTaskController::AddTask(
    const std::string& app_id, base::Closure task) {
  DelayedTaskMap::const_iterator iter = delayed_task_map_.find(app_id);
  DCHECK(iter != delayed_task_map_.end());
  iter->second->tasks.push_back(task);
}

void GCMProfileService::DelayedTaskController::SetGCMReady() {
  gcm_ready_ = true;

  for (DelayedTaskMap::iterator iter = delayed_task_map_.begin();
       iter != delayed_task_map_.end(); ++iter) {
    if (iter->second->ready)
      RunTasks(iter->second);
  }
}

void GCMProfileService::DelayedTaskController::SetAppReady(
    const std::string& app_id) {
  DelayedTaskMap::iterator iter = delayed_task_map_.find(app_id);
  DCHECK(iter != delayed_task_map_.end());

  AppTaskQueue* task_queue = iter->second;
  DCHECK(task_queue);
  task_queue->ready = true;

  if (gcm_ready_)
    RunTasks(task_queue);
}

bool GCMProfileService::DelayedTaskController::CanRunTaskWithoutDelay(
    const std::string& app_id) const {
  if (!gcm_ready_)
    return false;
  DelayedTaskMap::const_iterator iter = delayed_task_map_.find(app_id);
  if (iter == delayed_task_map_.end())
    return true;
  return iter->second->ready;
}

bool GCMProfileService::DelayedTaskController::IsAppTracked(
    const std::string& app_id) const {
  return delayed_task_map_.find(app_id) != delayed_task_map_.end();
}

void GCMProfileService::DelayedTaskController::RunTasks(
    AppTaskQueue* task_queue) {
  DCHECK(gcm_ready_ && task_queue->ready);

  for (size_t i = 0; i < task_queue->tasks.size(); ++i)
    task_queue->tasks[i].Run();
  task_queue->tasks.clear();
}

class GCMProfileService::IOWorker
    : public GCMClient::Delegate,
      public base::RefCountedThreadSafe<GCMProfileService::IOWorker>{
 public:
  // Called on UI thread.
  explicit IOWorker(const base::WeakPtr<GCMProfileService>& service);

  // Overridden from GCMClient::Delegate:
  // Called on IO thread.
  virtual void OnRegisterFinished(const std::string& app_id,
                                  const std::string& registration_id,
                                  GCMClient::Result result) OVERRIDE;
  virtual void OnUnregisterFinished(const std::string& app_id,
                                    bool success) OVERRIDE;
  virtual void OnSendFinished(const std::string& app_id,
                              const std::string& message_id,
                              GCMClient::Result result) OVERRIDE;
  virtual void OnMessageReceived(
      const std::string& app_id,
      const GCMClient::IncomingMessage& message) OVERRIDE;
  virtual void OnMessagesDeleted(const std::string& app_id) OVERRIDE;
  virtual void OnMessageSendError(const std::string& app_id,
                                  const std::string& message_id,
                                  GCMClient::Result result) OVERRIDE;
  virtual void OnGCMReady() OVERRIDE;

  // Called on IO thread.
  void Initialize(
      GCMClientFactory* gcm_client_factory,
      const base::FilePath& store_path,
      const scoped_refptr<net::URLRequestContextGetter>&
          url_request_context_getter);
  void Reset();
  void CheckOut();
  void Register(const std::string& app_id,
                const std::vector<std::string>& sender_ids,
                const std::string& cert);
  void Unregister(const std::string& app_id);
  void Send(const std::string& app_id,
            const std::string& receiver_id,
            const GCMClient::OutgoingMessage& message);

 private:
  friend class base::RefCountedThreadSafe<IOWorker>;
  virtual ~IOWorker();

  const base::WeakPtr<GCMProfileService> service_;

  scoped_ptr<GCMClient> gcm_client_;
};

GCMProfileService::IOWorker::IOWorker(
    const base::WeakPtr<GCMProfileService>& service)
    : service_(service) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

GCMProfileService::IOWorker::~IOWorker() {
}

void GCMProfileService::IOWorker::Initialize(
    GCMClientFactory* gcm_client_factory,
    const base::FilePath& store_path,
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
                          blocking_task_runner,
                          url_request_context_getter,
                          this);

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GCMProfileService::FinishInitializationOnUI,
                 service_,
                 gcm_client_->IsReady()));
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
    bool success) {
  // Nothing to do here.
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
    const std::string& message_id,
    GCMClient::Result result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GCMProfileService::MessageSendError,
                 service_,
                 app_id,
                 message_id,
                 result));
}

void GCMProfileService::IOWorker::OnGCMReady() {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GCMProfileService::GCMClientReady,
                 service_));
}

void GCMProfileService::IOWorker::CheckOut() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  gcm_client_->CheckOut();
  gcm_client_.reset();
}

void GCMProfileService::IOWorker::Register(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids,
    const std::string& cert) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  gcm_client_->Register(app_id, cert, sender_ids);
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

GCMProfileService::RegistrationInfo::RegistrationInfo() {
}

GCMProfileService::RegistrationInfo::~RegistrationInfo() {
}

bool GCMProfileService::RegistrationInfo::IsValid() const {
  return !sender_ids.empty() && !registration_id.empty();
}

bool GCMProfileService::enable_gcm_for_testing_ = false;

// static
bool GCMProfileService::IsGCMEnabled(Profile* profile) {
  // GCM is not enabled in incognito mode.
  if (profile->IsOffTheRecord())
    return false;

  if (enable_gcm_for_testing_)
    return true;

  return profile->GetPrefs()->GetBoolean(prefs::kGCMChannelEnabled);
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
  registry->RegisterListPref(
      prefs::kGCMRegisteredAppIDs,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

GCMProfileService::GCMProfileService(Profile* profile)
    : profile_(profile),
      gcm_client_ready_(false),
      testing_delegate_(NULL),
      weak_ptr_factory_(this) {
  DCHECK(!profile->IsOffTheRecord());
}

GCMProfileService::~GCMProfileService() {
}

void GCMProfileService::Initialize(
    scoped_ptr<GCMClientFactory> gcm_client_factory) {
  gcm_client_factory_ = gcm_client_factory.Pass();

  // This has to be done first since CheckIn depends on it.
  io_worker_ = new IOWorker(weak_ptr_factory_.GetWeakPtr());

#if !defined(OS_ANDROID)
  js_event_router_.reset(new extensions::GcmJsEventRouter(profile_));
#endif

  registrar_.Add(this,
                 chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
                 content::Source<Profile>(profile_));
  registrar_.Add(this,
                 chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
                 content::Source<Profile>(profile_));
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::Source<Profile>(profile_));
  // TODO(jianli): move extension specific logic out of GCMProfileService.
  registrar_.Add(this,
                 chrome:: NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<Profile>(profile_));

  // In case that the profile has been signed in before GCMProfileService is
  // created.
  SigninManagerBase* manager = SigninManagerFactory::GetForProfile(profile_);
  if (manager) {
    std::string username = manager->GetAuthenticatedUsername();
    if (!username.empty())
      CheckIn(username);
  }
}

void GCMProfileService::Register(const std::string& app_id,
                                 const std::vector<std::string>& sender_ids,
                                 const std::string& cert,
                                 RegisterCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!app_id.empty() && !sender_ids.empty() && !callback.is_null());

  // If the profile was not signed in, bail out.
  if (username_.empty()) {
    callback.Run(std::string(), GCMClient::NOT_SIGNED_IN);
    return;
  }

  // If previous register operation is still in progress, bail out.
  if (register_callbacks_.find(app_id) != register_callbacks_.end()) {
    callback.Run(std::string(), GCMClient::ASYNC_OPERATION_PENDING);
    return;
  }

  register_callbacks_[app_id] = callback;

  EnsureAppReady(app_id);

  // Delay the register operation until GCMClient is ready.
  if (!delayed_task_controller_->CanRunTaskWithoutDelay(app_id)) {
    delayed_task_controller_->AddTask(
        app_id,
        base::Bind(&GCMProfileService::DoRegister,
                   weak_ptr_factory_.GetWeakPtr(),
                   app_id,
                   sender_ids,
                   cert));
    return;
  }

  DoRegister(app_id, sender_ids, cert);
}

void GCMProfileService::DoRegister(const std::string& app_id,
                                   const std::vector<std::string>& sender_ids,
                                   const std::string& cert) {
  std::map<std::string, RegisterCallback>::iterator callback_iter =
      register_callbacks_.find(app_id);
  if (callback_iter == register_callbacks_.end()) {
    // The callback could have been removed when the app is uninstalled.
    return;
  }

  // Normalize the sender IDs by making them sorted.
  std::vector<std::string> normalized_sender_ids = sender_ids;
  std::sort(normalized_sender_ids.begin(), normalized_sender_ids.end());

  // If the same sender ids is provided, return the cached registration ID
  // directly.
  RegistrationInfoMap::const_iterator registration_info_iter =
      registration_info_map_.find(app_id);
  if (registration_info_iter != registration_info_map_.end() &&
      registration_info_iter->second.sender_ids == normalized_sender_ids) {
    RegisterCallback callback = callback_iter->second;
    register_callbacks_.erase(callback_iter);
    callback.Run(registration_info_iter->second.registration_id,
                 GCMClient::SUCCESS);
    return;
  }

  // Cache the sender IDs. The registration ID will be filled when the
  // registration completes.
  RegistrationInfo registration_info;
  registration_info.sender_ids = normalized_sender_ids;
  registration_info_map_[app_id] = registration_info;

  // Save the IDs of all registered apps such that we know what to remove from
  // the the app's state store when the profile is signed out.
  WriteRegisteredAppIDs();

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMProfileService::IOWorker::Register,
                 io_worker_,
                 app_id,
                 normalized_sender_ids,
                 cert));
}

void GCMProfileService::Send(const std::string& app_id,
                             const std::string& receiver_id,
                             const GCMClient::OutgoingMessage& message,
                             SendCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!app_id.empty() && !receiver_id.empty() && !callback.is_null());

  // If the profile was not signed in, bail out.
  if (username_.empty()) {
    callback.Run(std::string(), GCMClient::NOT_SIGNED_IN);
    return;
  }

  // If the message with send ID is still in progress, bail out.
  std::pair<std::string, std::string> key(app_id, message.id);
  if (send_callbacks_.find(key) != send_callbacks_.end()) {
    callback.Run(message.id, GCMClient::INVALID_PARAMETER);
    return;
  }
  send_callbacks_[key] = callback;

  EnsureAppReady(app_id);

  // Delay the send operation until all GCMClient is ready.
  if (!delayed_task_controller_->CanRunTaskWithoutDelay(app_id)) {
    delayed_task_controller_->AddTask(
        app_id,
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

void GCMProfileService::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  switch (type) {
    case chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL: {
      const GoogleServiceSigninSuccessDetails* signin_details =
          content::Details<GoogleServiceSigninSuccessDetails>(details).ptr();
      // This could be called multiple times when the password changed.
      if (username_ != signin_details->username)
        CheckIn(signin_details->username);
      break;
    }
    case chrome::NOTIFICATION_GOOGLE_SIGNED_OUT:
      CheckOut();
      break;
    case chrome::NOTIFICATION_PROFILE_DESTROYED:
      ResetGCMClient();
      break;
    case chrome:: NOTIFICATION_EXTENSION_UNINSTALLED: {
      extensions::Extension* extension =
          content::Details<extensions::Extension>(details).ptr();
      Unregister(extension->id());
      break;
    }
    default:
      NOTREACHED();
  }
}

void GCMProfileService::CheckIn(const std::string& username) {
  DCHECK(!username.empty() && username_.empty());
  username_ = username;

  DCHECK(!delayed_task_controller_);
  delayed_task_controller_.reset(new DelayedTaskController);

  // Load all register apps.
  ReadRegisteredAppIDs();

  // Let the IO thread create and initialize GCMClient.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter =
      profile_->GetRequestContext();
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMProfileService::IOWorker::Initialize,
                 io_worker_,
                 gcm_client_factory_.get(),
                 profile_->GetPath().Append(chrome::kGCMStoreDirname),
                 url_request_context_getter));
}

void GCMProfileService::CheckOut() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  DCHECK(!username_.empty());
  username_.clear();

  // Remove persisted data from app's state store.
  for (RegistrationInfoMap::const_iterator iter =
           registration_info_map_.begin();
       iter != registration_info_map_.end(); ++iter) {
    DeleteRegistrationInfo(iter->first);
  }

  // Remove persisted data from prefs store.
  profile_->GetPrefs()->ClearPref(prefs::kGCMChannelEnabled);
  profile_->GetPrefs()->ClearPref(prefs::kGCMRegisteredAppIDs);

  gcm_client_ready_ = false;
  delayed_task_controller_.reset();
  register_callbacks_.clear();
  send_callbacks_.clear();
  registration_info_map_.clear();

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

void GCMProfileService::EnsureAppReady(const std::string& app_id) {
  if (delayed_task_controller_->IsAppTracked(app_id))
    return;

  ReadRegistrationInfo(app_id);
}

void GCMProfileService::Unregister(const std::string& app_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // This is unlikely to happen because the app will not be uninstalled before
  // the asynchronous extension function completes.
  DCHECK(register_callbacks_.find(app_id) == register_callbacks_.end());

  // Remove the cached registration info. If not found, there is no need to
  // ask the server to unregister it.
  RegistrationInfoMap::iterator registration_info_iter =
      registration_info_map_.find(app_id);
  if (registration_info_iter == registration_info_map_.end())
    return;
  registration_info_map_.erase(registration_info_iter);

  // Update the persisted IDs of registered apps.
  WriteRegisteredAppIDs();

  // Remove the persisted registration info.
  DeleteRegistrationInfo(app_id);

  // No need to track the app any more.
  delayed_task_controller_->RemoveApp(app_id);

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

  // Cache the registration ID if the registration succeeds. Otherwise,
  // removed the cached info.
  RegistrationInfoMap::iterator registration_info_iter =
      registration_info_map_.find(app_id);
  // This is unlikely to happen because the app will not be uninstalled before
  // the asynchronous extension function completes.
  DCHECK(registration_info_iter != registration_info_map_.end());
  if (result == GCMClient::SUCCESS) {
    registration_info_iter->second.registration_id = registration_id;
    WriteRegistrationInfo(app_id);
  } else {
    registration_info_map_.erase(registration_info_iter);
  }

  RegisterCallback callback = callback_iter->second;
  register_callbacks_.erase(callback_iter);
  callback.Run(registration_id, result);
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

  GetEventRouter(app_id)->OnMessage(app_id, message);
}

void GCMProfileService::MessagesDeleted(const std::string& app_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Drop the event if signed out.
  if (username_.empty())
    return;

  GetEventRouter(app_id)->OnMessagesDeleted(app_id);
}

void GCMProfileService::MessageSendError(const std::string& app_id,
                                         const std::string& message_id,
                                         GCMClient::Result result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Drop the event if signed out.
  if (username_.empty())
    return;

  GetEventRouter(app_id)->OnSendError(app_id, message_id, result);
}

void GCMProfileService::FinishInitializationOnUI(bool ready) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  gcm_client_ready_ = ready;
  if (gcm_client_ready_)
    delayed_task_controller_->SetGCMReady();
}

void GCMProfileService::GCMClientReady() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (gcm_client_ready_)
    return;
  gcm_client_ready_ = true;

  delayed_task_controller_->SetGCMReady();
}

GCMEventRouter* GCMProfileService::GetEventRouter(const std::string& app_id)
    const {
  if (testing_delegate_ && testing_delegate_->GetEventRouter())
    return testing_delegate_->GetEventRouter();
#if defined(OS_ANDROID)
  return NULL;
#else
  return js_event_router_.get();
#endif
}

void GCMProfileService::ReadRegisteredAppIDs() {
  const base::ListValue* app_id_list =
      profile_->GetPrefs()->GetList(prefs::kGCMRegisteredAppIDs);
  for (size_t i = 0; i < app_id_list->GetSize(); ++i) {
    std::string app_id;
    if (!app_id_list->GetString(i, &app_id))
      continue;
    ReadRegistrationInfo(app_id);
  }
}

void GCMProfileService::WriteRegisteredAppIDs() {
  base::ListValue apps;
  for (RegistrationInfoMap::const_iterator iter =
           registration_info_map_.begin();
       iter != registration_info_map_.end(); ++iter) {
    apps.Append(new base::StringValue(iter->first));
  }
  profile_->GetPrefs()->Set(prefs::kGCMRegisteredAppIDs, apps);
}

void GCMProfileService::DeleteRegistrationInfo(const std::string& app_id) {
  extensions::StateStore* storage =
      extensions::ExtensionSystem::Get(profile_)->state_store();
  DCHECK(storage);

  storage->RemoveExtensionValue(app_id, kRegistrationKey);
}

void GCMProfileService::WriteRegistrationInfo(const std::string& app_id) {
  extensions::StateStore* storage =
      extensions::ExtensionSystem::Get(profile_)->state_store();
  DCHECK(storage);

  RegistrationInfoMap::const_iterator registration_info_iter =
      registration_info_map_.find(app_id);
  if (registration_info_iter == registration_info_map_.end())
    return;
  const RegistrationInfo& registration_info = registration_info_iter->second;

  scoped_ptr<base::ListValue> senders_list(new base::ListValue());
  for (std::vector<std::string>::const_iterator senders_iter =
           registration_info.sender_ids.begin();
       senders_iter != registration_info.sender_ids.end();
       ++senders_iter) {
    senders_list->AppendString(*senders_iter);
  }

  scoped_ptr<base::DictionaryValue> registration_info_dict(
      new base::DictionaryValue());
  registration_info_dict->Set(kSendersKey, senders_list.release());
  registration_info_dict->SetString(kRegistrationIDKey,
                                    registration_info.registration_id);

  storage->SetExtensionValue(
      app_id, kRegistrationKey, registration_info_dict.PassAs<base::Value>());
}

void GCMProfileService::ReadRegistrationInfo(const std::string& app_id) {
  delayed_task_controller_->AddApp(app_id);

  extensions::StateStore* storage =
      extensions::ExtensionSystem::Get(profile_)->state_store();
  DCHECK(storage);
  storage->GetExtensionValue(
      app_id,
      kRegistrationKey,
      base::Bind(
          &GCMProfileService::ReadRegistrationInfoFinished,
          weak_ptr_factory_.GetWeakPtr(),
          app_id));
}

void GCMProfileService::ReadRegistrationInfoFinished(
    const std::string& app_id,
    scoped_ptr<base::Value> value) {
  RegistrationInfo registration_info;
  if (value &&
     !ParsePersistedRegistrationInfo(value.Pass(), &registration_info)) {
    // Delete the persisted data if it is corrupted.
    DeleteRegistrationInfo(app_id);
  }

  if (registration_info.IsValid())
    registration_info_map_[app_id] = registration_info;

  delayed_task_controller_->SetAppReady(app_id);
}

bool GCMProfileService::ParsePersistedRegistrationInfo(
    scoped_ptr<base::Value> value,
    RegistrationInfo* registration_info) {
  base::DictionaryValue* dict = NULL;
  if (!value.get() || !value->GetAsDictionary(&dict))
    return false;

  if (!dict->GetString(kRegistrationIDKey, &registration_info->registration_id))
    return false;

  const base::ListValue* senders_list = NULL;
  if (!dict->GetList(kSendersKey, &senders_list) || !senders_list->GetSize())
    return false;
  for (size_t i = 0; i < senders_list->GetSize(); ++i) {
    std::string sender;
    if (!senders_list->GetString(i, &sender))
      return false;
    registration_info->sender_ids.push_back(sender);
  }

  return true;
}

// static
const char* GCMProfileService::GetPersistentRegisterKeyForTesting() {
  return kRegistrationKey;
}

}  // namespace gcm
