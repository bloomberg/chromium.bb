// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/gcm_profile_service.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/state_store.h"
#include "chrome/browser/services/gcm/gcm_event_router.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "components/webdata/encryptor/encryptor.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "extensions/common/extension.h"

using extensions::Extension;

namespace gcm {

const char kRegistrationKey[] = "gcm.registration";
const char kSendersKey[] = "senders";
const char kRegistrationIDKey[] = "reg_id";

// Helper class to save tasks to run until we're ready to execute them.
class GCMProfileService::DelayedTaskController {
 public:
  DelayedTaskController();
  ~DelayedTaskController();

  // Adds an app to non-ready list that queue all tasks to invoke until ready.
  void AddApp(const std::string& app_id);

  // Adds a task that will be invoked once we're ready.
  void AddTask(const std::string& app_id, base::Closure task);

  // Marks that GCMClient is ready.
  void SetGCMClientReady();

  // Marks that the app is ready to have operations performed.
  void SetAppReady(const std::string& app_id);

  // Returns true if it is ready to perform operations for an app.
  bool CanRunTaskWithoutDelay(const std::string& app_id) const;

 private:
  struct AppTaskQueue {
    AppTaskQueue();
    ~AppTaskQueue();

    // The flag that indicates if GCMProfileService completes loading the
    // persistent data for the app.
    bool ready;

    // Tasks to be invoked upon ready.
    std::vector<base::Closure> tasks;
  };

  void RunTasks(AppTaskQueue* task_queue);

  // Flag that indicates that GCMClient is ready.
  bool gcm_client_ready_;

  // Map from app_id to callbacks.
  typedef std::map<std::string, AppTaskQueue*> DelayedTaskMap;
  DelayedTaskMap delayed_task_map_;
};

GCMProfileService::DelayedTaskController::AppTaskQueue::AppTaskQueue()
    : ready(false) {
}

GCMProfileService::DelayedTaskController::AppTaskQueue::~AppTaskQueue() {
}

GCMProfileService::DelayedTaskController::DelayedTaskController()
    : gcm_client_ready_(false) {
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

void GCMProfileService::DelayedTaskController::AddTask(
    const std::string& app_id, base::Closure task) {
  DelayedTaskMap::const_iterator iter = delayed_task_map_.find(app_id);
  DCHECK(iter != delayed_task_map_.end());
  iter->second->tasks.push_back(task);
}

void GCMProfileService::DelayedTaskController::SetGCMClientReady() {
  gcm_client_ready_ = true;

  for (DelayedTaskMap::iterator iter = delayed_task_map_.begin();
       iter != delayed_task_map_.end();) {
    if (iter->second->ready) {
      AppTaskQueue* task_queue = iter->second;
      RunTasks(task_queue);
      delete task_queue;
      delayed_task_map_.erase(iter++);
    } else {
      ++iter;
    }
  }
}

void GCMProfileService::DelayedTaskController::SetAppReady(
    const std::string& app_id) {
  DelayedTaskMap::iterator iter = delayed_task_map_.find(app_id);
  DCHECK(iter != delayed_task_map_.end());

  AppTaskQueue* task_queue = iter->second;
  DCHECK(task_queue);
  task_queue->ready = true;

  if (gcm_client_ready_) {
    RunTasks(task_queue);
    delete task_queue;
    delayed_task_map_.erase(iter);
  }
}

bool GCMProfileService::DelayedTaskController::CanRunTaskWithoutDelay(
    const std::string& app_id) const {
  if (!gcm_client_ready_)
    return false;
  DelayedTaskMap::const_iterator iter = delayed_task_map_.find(app_id);
  if (iter == delayed_task_map_.end())
    return true;
  return iter->second->ready;
}

void GCMProfileService::DelayedTaskController::RunTasks(
    AppTaskQueue* task_queue) {
  DCHECK(gcm_client_ready_ && task_queue->ready);

  for (size_t i = 0; i < task_queue->tasks.size(); ++i)
    task_queue->tasks[i].Run();
  task_queue->tasks.clear();
}

class GCMProfileService::IOWorker
    : public GCMClient::Delegate,
      public base::RefCountedThreadSafe<GCMProfileService::IOWorker>{
 public:
  explicit IOWorker(const base::WeakPtr<GCMProfileService>& service);

  // Overridden from GCMClient::Delegate:
  // Called from IO thread.
  virtual void OnCheckInFinished(const GCMClient::CheckInInfo& checkin_info,
                                 GCMClient::Result result) OVERRIDE;
  virtual void OnRegisterFinished(const std::string& app_id,
                                  const std::string& registration_id,
                                  GCMClient::Result result) OVERRIDE;
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
  virtual GCMClient::CheckInInfo GetCheckInInfo() const OVERRIDE;
  virtual void OnLoadingCompleted() OVERRIDE;
  virtual base::TaskRunner* GetFileTaskRunner() OVERRIDE;

  void CheckGCMClientLoading();
  void SetUser(const std::string& username);
  void RemoveUser(const std::string& username);
  void CheckIn();
  void SetCheckInInfo(GCMClient::CheckInInfo checkin_info);
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

  // The username (email address) of the signed-in user.
  std::string username_;

  // The checkin info obtained from the server for the signed in user associated
  // with the profile.
  GCMClient::CheckInInfo checkin_info_;
};

GCMProfileService::IOWorker::IOWorker(
    const base::WeakPtr<GCMProfileService>& service)
    : service_(service) {
}

GCMProfileService::IOWorker::~IOWorker() {
}

void GCMProfileService::IOWorker::OnCheckInFinished(
    const GCMClient::CheckInInfo& checkin_info,
    GCMClient::Result result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  checkin_info_ = checkin_info;

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GCMProfileService::CheckInFinished,
                 service_,
                 checkin_info_,
                 result));
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

GCMClient::CheckInInfo GCMProfileService::IOWorker::GetCheckInInfo() const {
  return checkin_info_;
}

void GCMProfileService::IOWorker::OnLoadingCompleted() {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GCMProfileService::GCMClientLoadingFinished,
                 service_));
}

base::TaskRunner* GCMProfileService::IOWorker::GetFileTaskRunner() {
  // TODO(jianli): to be implemented.
  return NULL;
}

void GCMProfileService::IOWorker::CheckGCMClientLoading() {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GCMProfileService::CheckGCMClientLoadingFinished,
                 service_,
                 GCMClient::Get()->IsLoading()));
}

void GCMProfileService::IOWorker::SetUser(const std::string& username) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(username_.empty() && !username.empty());

  username_ = username;
  GCMClient::Get()->SetUserDelegate(username_, this);
}

void GCMProfileService::IOWorker::RemoveUser(const std::string& username) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(!username.empty());

  if (username_.empty())
    return;
  username_.clear();
  GCMClient::Get()->SetUserDelegate(username_, NULL);
}

void GCMProfileService::IOWorker::CheckIn() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  GCMClient::Get()->CheckIn(username_);
}

void GCMProfileService::IOWorker::SetCheckInInfo(
    GCMClient::CheckInInfo checkin_info) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  checkin_info_ = checkin_info;
}

void GCMProfileService::IOWorker::CheckOut() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  username_.clear();
  checkin_info_.Reset();
}

void GCMProfileService::IOWorker::Register(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids,
    const std::string& cert) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(!username_.empty() && checkin_info_.IsValid());

  GCMClient::Get()->Register(username_, app_id, cert, sender_ids);
}

void GCMProfileService::IOWorker::Unregister(const std::string& app_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(!username_.empty() && checkin_info_.IsValid());

  GCMClient::Get()->Unregister(username_, app_id);
}

void GCMProfileService::IOWorker::Send(
    const std::string& app_id,
    const std::string& receiver_id,
    const GCMClient::OutgoingMessage& message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(!username_.empty() && checkin_info_.IsValid());

  GCMClient::Get()->Send(username_, app_id, receiver_id, message);
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
bool GCMProfileService::IsGCMEnabled() {
  if (enable_gcm_for_testing_)
    return true;

  // GCM support is only enabled for Canary/Dev builds.
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  return channel == chrome::VersionInfo::CHANNEL_UNKNOWN ||
         channel == chrome::VersionInfo::CHANNEL_CANARY ||
         channel == chrome::VersionInfo::CHANNEL_DEV;
}

// static
void GCMProfileService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterUint64Pref(
      prefs::kGCMUserAccountID,
      0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kGCMUserToken,
      "",
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

GCMProfileService::GCMProfileService(Profile* profile)
    : profile_(profile),
      testing_delegate_(NULL),
      weak_ptr_factory_(this) {
  Init();
}

GCMProfileService::GCMProfileService(Profile* profile,
                                     TestingDelegate* testing_delegate)
    : profile_(profile),
      testing_delegate_(testing_delegate),
      weak_ptr_factory_(this) {
  Init();
}

GCMProfileService::~GCMProfileService() {
  if (username_.empty())
    return;
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMProfileService::IOWorker::RemoveUser,
                 io_worker_,
                 username_));
}

void GCMProfileService::Init() {
  delayed_task_controller_.reset(new DelayedTaskController);

  // This has to be done first since CheckIn depends on it.
  io_worker_ = new IOWorker(weak_ptr_factory_.GetWeakPtr());

  // Checks if GCMClient has finished the loading.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMProfileService::IOWorker::CheckGCMClientLoading,
                 io_worker_));

  // In case that the profile has been signed in before GCMProfileService is
  // created.
  SigninManagerBase* manager = SigninManagerFactory::GetForProfile(profile_);
  if (manager)
    AddUser(manager->GetAuthenticatedUsername());

  registrar_.Add(this,
                 chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
                 content::Source<Profile>(profile_));
  registrar_.Add(this,
                 chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
                 content::Source<Profile>(profile_));
  // TODO(jianli): move extension specific logic out of GCMProfileService.
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this,
                 chrome:: NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<Profile>(profile_));
}

void GCMProfileService::Register(const std::string& app_id,
                                 const std::vector<std::string>& sender_ids,
                                 const std::string& cert,
                                 RegisterCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!app_id.empty() && !sender_ids.empty() && !callback.is_null());

  // If previous register operation is still in progress, bail out.
  if (register_callbacks_.find(app_id) != register_callbacks_.end()) {
    callback.Run(std::string(), GCMClient::ASYNC_OPERATION_PENDING);
    return;
  }

  // Delay the register operation until the loading is done.
  if (!delayed_task_controller_->CanRunTaskWithoutDelay(app_id)) {
    delayed_task_controller_->AddTask(
        app_id,
        base::Bind(&GCMProfileService::DoRegister,
                   weak_ptr_factory_.GetWeakPtr(),
                   app_id,
                   sender_ids,
                   cert,
                   callback));
    return;
  }

  DoRegister(app_id, sender_ids, cert, callback);
}

void GCMProfileService::DoRegister(const std::string& app_id,
                                   const std::vector<std::string>& sender_ids,
                                   const std::string& cert,
                                   RegisterCallback callback) {
  // Normalize the sender IDs by making them sorted.
  std::vector<std::string> normalized_sender_ids = sender_ids;
  std::sort(normalized_sender_ids.begin(), normalized_sender_ids.end());

  // If the same sender ids is provided, return the cached registration ID
  // directly.
  RegistrationInfoMap::const_iterator registration_info_iter =
      registration_info_map_.find(app_id);
  if (registration_info_iter != registration_info_map_.end() &&
      registration_info_iter->second.sender_ids == normalized_sender_ids) {
    callback.Run(registration_info_iter->second.registration_id,
                 GCMClient::SUCCESS);
    return;
  }

  // Cache the sender IDs. The registration ID will be filled when the
  // registration completes.
  RegistrationInfo registration_info;
  registration_info.sender_ids = normalized_sender_ids;
  registration_info_map_[app_id] = registration_info;

  register_callbacks_[app_id] = callback;

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

  // If the message with send ID is still in progress, bail out.
  std::pair<std::string, std::string> key(app_id, message.id);
  if (send_callbacks_.find(key) != send_callbacks_.end()) {
    callback.Run(message.id, GCMClient::INVALID_PARAMETER);
    return;
  }
  send_callbacks_[key] = callback;

  // Delay the send operation until all the loadings are done.
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
      AddUser(signin_details->username);
      break;
    }
    case chrome::NOTIFICATION_GOOGLE_SIGNED_OUT:
      username_.clear();
      RemoveUser();
      break;
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      extensions::Extension* extension =
          content::Details<extensions::Extension>(details).ptr();
      // No need to load the persisted registration info if the extension does
      // not have the GCM permission.
      if (extension->HasAPIPermission(extensions::APIPermission::kGcm))
        ReadRegistrationInfo(extension->id());
      break;
    }
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

void GCMProfileService::AddUser(const std::string& username) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // If re-signin occurs due to password change, there is no need to do
  // check-in again.
  if (username_ == username || username.empty())
    return;
  username_ = username;

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMProfileService::IOWorker::SetUser,
                 io_worker_,
                 username_));

  // Try to read persisted check-in info from the profile's prefs store.
  PrefService* pref_service = profile_->GetPrefs();
  uint64 android_id = pref_service->GetUint64(prefs::kGCMUserAccountID);
  std::string base64_token = pref_service->GetString(prefs::kGCMUserToken);
  std::string encrypted_secret;
  base::Base64Decode(base::StringPiece(base64_token), &encrypted_secret);
  if (android_id && !encrypted_secret.empty()) {
    std::string decrypted_secret;
    Encryptor::DecryptString(encrypted_secret, &decrypted_secret);
    uint64 secret = 0;
    if (base::StringToUint64(decrypted_secret, &secret) && secret) {
      GCMClient::CheckInInfo checkin_info;
      checkin_info.android_id = android_id;
      checkin_info.secret = secret;
      content::BrowserThread::PostTask(
          content::BrowserThread::IO,
          FROM_HERE,
          base::Bind(&GCMProfileService::IOWorker::SetCheckInInfo,
                     io_worker_,
                     checkin_info));

      if (testing_delegate_)
        testing_delegate_->CheckInFinished(checkin_info, GCMClient::SUCCESS);

      return;
    }
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMProfileService::IOWorker::CheckIn, io_worker_));
}

void GCMProfileService::RemoveUser() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->ClearPref(prefs::kGCMUserAccountID);
  pref_service->ClearPref(prefs::kGCMUserToken);

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMProfileService::IOWorker::CheckOut, io_worker_));
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

  // Remove the persisted registration info.
  DeleteRegistrationInfo(app_id);

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

void GCMProfileService::CheckInFinished(GCMClient::CheckInInfo checkin_info,
                                        GCMClient::Result result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Save the check-in info into the profile's prefs store.
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetUint64(prefs::kGCMUserAccountID, checkin_info.android_id);

  // Encrypt the secret for persisting purpose.
  std::string encrypted_secret;
  Encryptor::EncryptString(base::Uint64ToString(checkin_info.secret),
                           &encrypted_secret);

  // |encrypted_secret| might contain binary data and our prefs store only
  // works for the text.
  std::string base64_token;
  base::Base64Encode(encrypted_secret, &base64_token);
  pref_service->SetString(prefs::kGCMUserToken, base64_token);

  if (testing_delegate_)
    testing_delegate_->CheckInFinished(checkin_info, result);
}

void GCMProfileService::RegisterFinished(std::string app_id,
                                         std::string registration_id,
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

void GCMProfileService::SendFinished(std::string app_id,
                                     std::string message_id,
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

void GCMProfileService::MessageReceived(std::string app_id,
                                        GCMClient::IncomingMessage message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  GetEventRouter(app_id)->OnMessage(app_id, message);
}

void GCMProfileService::MessagesDeleted(std::string app_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  GetEventRouter(app_id)->OnMessagesDeleted(app_id);
}

void GCMProfileService::MessageSendError(std::string app_id,
                                         std::string message_id,
                                         GCMClient::Result result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  GetEventRouter(app_id)->OnSendError(app_id, message_id, result);
}

void GCMProfileService::CheckGCMClientLoadingFinished(bool is_loading) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (!is_loading)
    delayed_task_controller_->SetGCMClientReady();
}

void GCMProfileService::GCMClientLoadingFinished() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  delayed_task_controller_->SetGCMClientReady();
}

GCMEventRouter* GCMProfileService::GetEventRouter(const std::string& app_id) {
  if (testing_delegate_ && testing_delegate_->GetEventRouter())
    return testing_delegate_->GetEventRouter();
  // TODO(fgorski): check and create the event router for JS routing.
  return js_event_router_.get();
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
  extensions::StateStore* storage =
      extensions::ExtensionSystem::Get(profile_)->state_store();
  DCHECK(storage);

  delayed_task_controller_->AddApp(app_id);

  storage->GetExtensionValue(
      app_id,
      kRegistrationKey,
      base::Bind(
          &GCMProfileService::ReadRegistrationInfoFinished,
          weak_ptr_factory_.GetWeakPtr(),
          app_id));
}

void GCMProfileService::ReadRegistrationInfoFinished(
    std::string app_id,
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
