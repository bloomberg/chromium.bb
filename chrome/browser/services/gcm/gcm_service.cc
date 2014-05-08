// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/gcm_service.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/services/gcm/gcm_app_handler.h"
#include "chrome/browser/services/gcm/gcm_client_factory.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/oauth2_token_service.h"
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
class GCMService::DelayedTaskController {
 public:
  DelayedTaskController();
  ~DelayedTaskController();

  // Adds a task that will be invoked once we're ready.
  void AddTask(const base::Closure& task);

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

  DISALLOW_COPY_AND_ASSIGN(DelayedTaskController);
};

GCMService::DelayedTaskController::DelayedTaskController() : ready_(false) {
}

GCMService::DelayedTaskController::~DelayedTaskController() {
}

void GCMService::DelayedTaskController::AddTask(const base::Closure& task) {
  delayed_tasks_.push_back(task);
}

void GCMService::DelayedTaskController::SetReady() {
  ready_ = true;
  RunTasks();
}

bool GCMService::DelayedTaskController::CanRunTaskWithoutDelay() const {
  return ready_;
}

void GCMService::DelayedTaskController::RunTasks() {
  DCHECK(ready_);

  for (size_t i = 0; i < delayed_tasks_.size(); ++i)
    delayed_tasks_[i].Run();
  delayed_tasks_.clear();
}

class GCMService::IOWorker : public GCMClient::Delegate {
 public:
  // Called on UI thread.
  IOWorker();
  virtual ~IOWorker();

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
  void Load(const base::WeakPtr<GCMService>& service);
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
  base::WeakPtr<GCMService> service_;

  scoped_ptr<GCMClient> gcm_client_;

  DISALLOW_COPY_AND_ASSIGN(IOWorker);
};

GCMService::IOWorker::IOWorker() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

GCMService::IOWorker::~IOWorker() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
}

void GCMService::IOWorker::Initialize(
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

void GCMService::IOWorker::OnRegisterFinished(
    const std::string& app_id,
    const std::string& registration_id,
    GCMClient::Result result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  content::BrowserThread::PostTask(content::BrowserThread::UI,
                                   FROM_HERE,
                                   base::Bind(&GCMService::RegisterFinished,
                                              service_,
                                              app_id,
                                              registration_id,
                                              result));
}

void GCMService::IOWorker::OnUnregisterFinished(const std::string& app_id,
                                                GCMClient::Result result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GCMService::UnregisterFinished, service_, app_id, result));
}

void GCMService::IOWorker::OnSendFinished(const std::string& app_id,
                                          const std::string& message_id,
                                          GCMClient::Result result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  content::BrowserThread::PostTask(content::BrowserThread::UI,
                                   FROM_HERE,
                                   base::Bind(&GCMService::SendFinished,
                                              service_,
                                              app_id,
                                              message_id,
                                              result));
}

void GCMService::IOWorker::OnMessageReceived(
    const std::string& app_id,
    const GCMClient::IncomingMessage& message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  content::BrowserThread::PostTask(content::BrowserThread::UI,
                                   FROM_HERE,
                                   base::Bind(&GCMService::MessageReceived,
                                              service_,
                                              app_id,
                                              message));
}

void GCMService::IOWorker::OnMessagesDeleted(const std::string& app_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  content::BrowserThread::PostTask(content::BrowserThread::UI,
                                   FROM_HERE,
                                   base::Bind(&GCMService::MessagesDeleted,
                                              service_,
                                              app_id));
}

void GCMService::IOWorker::OnMessageSendError(
    const std::string& app_id,
    const GCMClient::SendErrorDetails& send_error_details) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  content::BrowserThread::PostTask(content::BrowserThread::UI,
                                   FROM_HERE,
                                   base::Bind(&GCMService::MessageSendError,
                                              service_,
                                              app_id,
                                              send_error_details));
}

void GCMService::IOWorker::OnGCMReady() {
  content::BrowserThread::PostTask(content::BrowserThread::UI,
                                   FROM_HERE,
                                   base::Bind(&GCMService::GCMClientReady,
                                              service_));
}

void GCMService::IOWorker::Load(const base::WeakPtr<GCMService>& service) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  service_ = service;
  gcm_client_->Load();
}

void GCMService::IOWorker::Stop() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  gcm_client_->Stop();
}

void GCMService::IOWorker::CheckOut() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  gcm_client_->CheckOut();

  // Note that we still need to keep GCMClient instance alive since the
  // GCMService may check in again.
}

void GCMService::IOWorker::Register(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  gcm_client_->Register(app_id, sender_ids);
}

void GCMService::IOWorker::Unregister(const std::string& app_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  gcm_client_->Unregister(app_id);
}

void GCMService::IOWorker::Send(const std::string& app_id,
                                const std::string& receiver_id,
                                const GCMClient::OutgoingMessage& message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  gcm_client_->Send(app_id, receiver_id, message);
}

void GCMService::IOWorker::GetGCMStatistics(bool clear_logs) {
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
      base::Bind(&GCMService::GetGCMStatisticsFinished, service_, stats));
}

void GCMService::IOWorker::SetGCMRecording(bool recording) {
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
      base::Bind(&GCMService::GetGCMStatisticsFinished, service_, stats));
}

GCMService::GCMService(scoped_ptr<IdentityProvider> identity_provider)
    : identity_provider_(identity_provider.Pass()),
      gcm_client_ready_(false),
      weak_ptr_factory_(this) {
}

GCMService::~GCMService() {
}

void GCMService::Initialize(scoped_ptr<GCMClientFactory> gcm_client_factory) {
  // Get the list of available accounts.
  std::vector<std::string> account_ids;
#if !defined(OS_ANDROID)
  account_ids = identity_provider_->GetTokenService()->GetAccounts();
#endif

  // Create and initialize the GCMClient. Note that this does not initiate the
  // GCM check-in.
  DCHECK(!io_worker_);
  io_worker_.reset(new IOWorker());
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMService::IOWorker::Initialize,
                 base::Unretained(io_worker_.get()),
                 base::Passed(&gcm_client_factory),
                 GetStorePath(),
                 account_ids,
                 GetURLRequestContextGetter()));

  // Load from the GCM store and initiate the GCM check-in if the rollout signal
  // indicates yes.
  if (ShouldStartAutomatically())
    EnsureLoaded();

  identity_provider_->AddObserver(this);
}

void GCMService::Start() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  EnsureLoaded();
}

void GCMService::Stop() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // No need to stop GCM service if not started yet.
  if (account_id_.empty())
    return;

  RemoveCachedData();

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMService::IOWorker::Stop,
                 base::Unretained(io_worker_.get())));
}

void GCMService::ShutdownService() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  identity_provider_->RemoveObserver(this);
  for (GCMAppHandlerMap::const_iterator iter = app_handlers_.begin();
       iter != app_handlers_.end(); ++iter) {
    iter->second->ShutdownHandler();
  }
  app_handlers_.clear();
  content::BrowserThread::DeleteSoon(content::BrowserThread::IO,
                                     FROM_HERE,
                                     io_worker_.release());
}

void GCMService::AddAppHandler(const std::string& app_id,
                               GCMAppHandler* handler) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!app_id.empty());
  DCHECK(handler);
  DCHECK(app_handlers_.find(app_id) == app_handlers_.end());

  app_handlers_[app_id] = handler;
}

void GCMService::RemoveAppHandler(const std::string& app_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!app_id.empty());

  app_handlers_.erase(app_id);
}

void GCMService::Register(const std::string& app_id,
                          const std::vector<std::string>& sender_ids,
                          RegisterCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!app_id.empty());
  DCHECK(!sender_ids.empty());
  DCHECK(!callback.is_null());

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
    delayed_task_controller_->AddTask(base::Bind(&GCMService::DoRegister,
                                                 weak_ptr_factory_.GetWeakPtr(),
                                                 app_id,
                                                 sender_ids));
    return;
  }

  DoRegister(app_id, sender_ids);
}

void GCMService::DoRegister(const std::string& app_id,
                            const std::vector<std::string>& sender_ids) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
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
      base::Bind(&GCMService::IOWorker::Register,
                 base::Unretained(io_worker_.get()),
                 app_id,
                 normalized_sender_ids));
}

void GCMService::Unregister(const std::string& app_id,
                            UnregisterCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!app_id.empty());
  DCHECK(!callback.is_null());

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
    delayed_task_controller_->AddTask(base::Bind(&GCMService::DoUnregister,
                                                 weak_ptr_factory_.GetWeakPtr(),
                                                 app_id));
    return;
  }

  DoUnregister(app_id);
}

void GCMService::DoUnregister(const std::string& app_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Ask the server to unregister it. There could be a small chance that the
  // unregister request fails. If this occurs, it does not bring any harm since
  // we simply reject the messages/events received from the server.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMService::IOWorker::Unregister,
                 base::Unretained(io_worker_.get()),
                 app_id));
}

void GCMService::Send(const std::string& app_id,
                      const std::string& receiver_id,
                      const GCMClient::OutgoingMessage& message,
                      SendCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!app_id.empty());
  DCHECK(!receiver_id.empty());
  DCHECK(!callback.is_null());

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
    delayed_task_controller_->AddTask(base::Bind(&GCMService::DoSend,
                                                 weak_ptr_factory_.GetWeakPtr(),
                                                 app_id,
                                                 receiver_id,
                                                 message));
    return;
  }

  DoSend(app_id, receiver_id, message);
}

void GCMService::DoSend(const std::string& app_id,
                        const std::string& receiver_id,
                        const GCMClient::OutgoingMessage& message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMService::IOWorker::Send,
                 base::Unretained(io_worker_.get()),
                 app_id,
                 receiver_id,
                 message));
}

GCMClient* GCMService::GetGCMClientForTesting() const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return io_worker_ ? io_worker_->gcm_client_for_testing() : NULL;
}

bool GCMService::IsStarted() const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return !account_id_.empty();
}

bool GCMService::IsGCMClientReady() const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return gcm_client_ready_;
}

void GCMService::GetGCMStatistics(GetGCMStatisticsCallback callback,
                                  bool clear_logs) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!callback.is_null());

  request_gcm_statistics_callback_ = callback;
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMService::IOWorker::GetGCMStatistics,
                 base::Unretained(io_worker_.get()),
                 clear_logs));
}

void GCMService::SetGCMRecording(GetGCMStatisticsCallback callback,
                                 bool recording) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  request_gcm_statistics_callback_ = callback;
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMService::IOWorker::SetGCMRecording,
                 base::Unretained(io_worker_.get()),
                 recording));
}

void GCMService::OnActiveAccountLogin() {
  if (ShouldStartAutomatically())
    EnsureLoaded();
}

void GCMService::OnActiveAccountLogout() {
  CheckOut();
}

void GCMService::EnsureLoaded() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  const std::string account_id = identity_provider_->GetActiveAccountId();
  if (account_id.empty())
    return;

  // CheckIn could be called more than once when:
  // 1) The password changes.
  // 2) Register/send function calls it to ensure CheckIn is done.
  if (account_id_ == account_id)
    return;
  account_id_ = account_id;

  DCHECK(!delayed_task_controller_);
  delayed_task_controller_.reset(new DelayedTaskController);

  // This will load the data from the gcm store and trigger the check-in if
  // the persisted check-in info is not found.
  // Note that we need to pass weak pointer again since the existing weak
  // pointer in IOWorker might have been invalidated when check-out occurs.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMService::IOWorker::Load,
                 base::Unretained(io_worker_.get()),
                 weak_ptr_factory_.GetWeakPtr()));
}

void GCMService::RemoveCachedData() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // Remove all the queued tasks since they no longer make sense after
  // GCM service is stopped.
  weak_ptr_factory_.InvalidateWeakPtrs();

  account_id_.clear();
  gcm_client_ready_ = false;
  delayed_task_controller_.reset();
  register_callbacks_.clear();
  send_callbacks_.clear();
}

void GCMService::CheckOut() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // We still proceed with the check-out logic even if the check-in is not
  // initiated in the current session. This will make sure that all the
  // persisted data written previously will get purged.

  RemoveCachedData();

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMService::IOWorker::CheckOut,
                 base::Unretained(io_worker_.get())));
}

GCMClient::Result GCMService::EnsureAppReady(const std::string& app_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // Ensure that check-in has been done.
  EnsureLoaded();

  // If the service was not started, bail out.
  if (account_id_.empty())
    return GCMClient::NOT_SIGNED_IN;

  return GCMClient::SUCCESS;
}

bool GCMService::IsAsyncOperationPending(const std::string& app_id) const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return register_callbacks_.find(app_id) != register_callbacks_.end() ||
         unregister_callbacks_.find(app_id) != unregister_callbacks_.end();
}

void GCMService::RegisterFinished(const std::string& app_id,
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

void GCMService::UnregisterFinished(const std::string& app_id,
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

void GCMService::SendFinished(const std::string& app_id,
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

void GCMService::MessageReceived(const std::string& app_id,
                                 GCMClient::IncomingMessage message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Drop the event if signed out.
  if (account_id_.empty())
    return;

  GetAppHandler(app_id)->OnMessage(app_id, message);
}

void GCMService::MessagesDeleted(const std::string& app_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Drop the event if signed out.
  if (account_id_.empty())
    return;

  GetAppHandler(app_id)->OnMessagesDeleted(app_id);
}

void GCMService::MessageSendError(
    const std::string& app_id,
    const GCMClient::SendErrorDetails& send_error_details) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Drop the event if signed out.
  if (account_id_.empty())
    return;

  GetAppHandler(app_id)->OnSendError(app_id, send_error_details);
}

void GCMService::GCMClientReady() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (gcm_client_ready_)
    return;
  gcm_client_ready_ = true;

  delayed_task_controller_->SetReady();
}

GCMAppHandler* GCMService::GetAppHandler(const std::string& app_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  std::map<std::string, GCMAppHandler*>::const_iterator iter =
      app_handlers_.find(app_id);
  return iter == app_handlers_.end() ? &default_app_handler_ : iter->second;
}

void GCMService::GetGCMStatisticsFinished(
    GCMClient::GCMStatistics stats) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  request_gcm_statistics_callback_.Run(stats);
}

}  // namespace gcm
