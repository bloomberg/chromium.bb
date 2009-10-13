// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef CHROME_PERSONALIZATION

#include "build/build_config.h"
#include "base/file_version_info.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/browser/sync/glue/http_bridge.h"
#include "chrome/browser/sync/glue/bookmark_model_worker.h"
#include "webkit/glue/webkit_glue.h"

static const int kSaveChangesIntervalSeconds = 10;
static const char kGaiaServiceId[] = "chromiumsync";
static const char kGaiaSourceForChrome[] = "ChromiumBrowser";
static const FilePath::CharType kSyncDataFolderName[] =
    FILE_PATH_LITERAL("Sync Data");

namespace browser_sync {

SyncBackendHost::SyncBackendHost(SyncFrontend* frontend,
                                 const FilePath& profile_path,
                                 ChangeProcessingInterface* processor)
    : core_thread_("Chrome_SyncCoreThread"),
      frontend_loop_(MessageLoop::current()),
      bookmark_model_worker_(NULL),
      frontend_(frontend),
      processor_(processor),
      sync_data_folder_path_(profile_path.Append(kSyncDataFolderName)),
      last_auth_error_(AUTH_ERROR_NONE) {
  core_ = new Core(this);
}

SyncBackendHost::~SyncBackendHost() {
  DCHECK(!core_ && !frontend_) << "Must call Shutdown before destructor.";
}

void SyncBackendHost::Initialize(const GURL& sync_service_url,
                                 URLRequestContext* baseline_context) {
  if (!core_thread_.Start())
    return;
  bookmark_model_worker_ = new BookmarkModelWorker(frontend_loop_);
  core_.get()->SetBaseRequestContext(baseline_context);
  core_thread_.message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(core_.get(), &SyncBackendHost::Core::DoInitialize,
                        sync_service_url, bookmark_model_worker_, true));
}

void SyncBackendHost::Authenticate(const std::string& username,
                                   const std::string& password) {
  core_thread_.message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(core_.get(), &SyncBackendHost::Core::DoAuthenticate,
                        username, password));
}

void SyncBackendHost::Shutdown(bool sync_disabled) {
  // Thread shutdown should occur in the following order:
  // - SyncerThread
  // - CoreThread
  // - UI Thread (stops some time after we return from this call).
  core_thread_.message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(core_.get(),
                        &SyncBackendHost::Core::DoShutdown,
                        sync_disabled));

  // Before joining the core_thread_, we wait for the BookmarkModelWorker to
  // give us the green light that it is not depending on the frontend_loop_ to
  // process any more tasks. Stop() blocks until this termination condition
  // is true.
  bookmark_model_worker_->Stop();

  // Stop will return once the thread exits, which will be after DoShutdown
  // runs. DoShutdown needs to run from core_thread_ because the sync backend
  // requires any thread that opened sqlite handles to relinquish them
  // personally. We need to join threads, because otherwise the main Chrome
  // thread (ui loop) can exit before DoShutdown finishes, at which point
  // virtually anything the sync backend does (or the post-back to
  // frontend_loop_ by our Core) will epically fail because the CRT won't be
  // initialized. For now this only ever happens at sync-enabled-Chrome exit,
  // meaning bug 1482548 applies to prolonged "waiting" that may occur in
  // DoShutdown.
  core_thread_.Stop();

  bookmark_model_worker_ = NULL;
  frontend_ = NULL;
  core_ = NULL;  // Releases reference to core_.
}

void SyncBackendHost::Core::NotifyFrontend(FrontendNotification notification) {
  if (!host_ || !host_->frontend_) {
    return;  // This can happen in testing because the UI loop processes tasks
             // after an instance of SyncBackendHost was destroyed.  In real
             // life this doesn't happen.
  }
  switch (notification) {
    case INITIALIZED:
      host_->frontend_->OnBackendInitialized();
      return;
    case SYNC_CYCLE_COMPLETED:
      host_->frontend_->OnSyncCycleCompleted();
      return;
  }
}

SyncBackendHost::UserShareHandle SyncBackendHost::GetUserShareHandle() const {
  return core_->syncapi()->GetUserShare();
}

SyncBackendHost::Status SyncBackendHost::GetDetailedStatus() {
  return core_->syncapi()->GetDetailedStatus();
}

SyncBackendHost::StatusSummary SyncBackendHost::GetStatusSummary() {
  return core_->syncapi()->GetStatusSummary();
}

string16 SyncBackendHost::GetAuthenticatedUsername() const {
  return UTF8ToUTF16(core_->syncapi()->GetAuthenticatedUsername());
}

AuthErrorState SyncBackendHost::GetAuthErrorState() const {
  return last_auth_error_;
}

void SyncBackendHost::Core::SetBaseRequestContext(
    URLRequestContext* request_context) {
  DCHECK(base_request_context_ == NULL);
  base_request_context_ = request_context;
  // This ref is removed on the IO thread after the core thread is over.
  base_request_context_->AddRef();
}

SyncBackendHost::Core::Core(SyncBackendHost* backend)
    : host_(backend),
      base_request_context_(NULL),
      syncapi_(new sync_api::SyncManager()) {
}

SyncBackendHost::Core::~Core() {
  if (base_request_context_) {
    ChromeThread::GetMessageLoop(ChromeThread::IO)->ReleaseSoon(FROM_HERE,
        base_request_context_);
    base_request_context_ = NULL;
  }
}

// Helper to construct a user agent string (ASCII) suitable for use by
// the syncapi for any HTTP communication. This string is used by the sync
// backend for classifying client types when calculating statistics.
std::string MakeUserAgentForSyncapi() {
  std::string user_agent;
  user_agent = "Chrome ";
#if defined(OS_WIN)
  user_agent += "WIN ";
#elif defined(OS_LINUX)
  user_agent += "LINUX ";
#elif defined(OS_MACOSX)
  user_agent += "MAC ";
#endif
  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfoForCurrentModule());
  if (version_info == NULL) {
    DLOG(ERROR) << "Unable to create FileVersionInfo object";
    return user_agent;
  }

  user_agent += WideToASCII(version_info->product_version());
  user_agent += " (" + WideToASCII(version_info->last_change()) + ")";
  if (!version_info->is_official_build())
    user_agent += "-devel";
  return user_agent;
}

void SyncBackendHost::Core::DoInitialize(
    const GURL& service_url,
    BookmarkModelWorker* bookmark_model_worker,
    bool attempt_last_user_authentication) {
  DCHECK(MessageLoop::current() == host_->core_thread_.message_loop());

  // Make sure that the directory exists before initializing the backend.
  // If it already exists, this will do no harm.
  bool success = file_util::CreateDirectory(host_->sync_data_folder_path());
  DCHECK(success);

  syncapi_->SetObserver(this);
  string16 path_str;
#if defined (OS_WIN)
  path_str = host_->sync_data_folder_path().value();
#elif (defined(OS_LINUX) || defined(OS_MACOSX))
  path_str = UTF8ToUTF16(host_->sync_data_folder_path().value());
#endif
  success = syncapi_->Init(path_str.c_str(),
      (service_url.host() + service_url.path()).c_str(),
      service_url.EffectiveIntPort(),
      kGaiaServiceId,
      kGaiaSourceForChrome,
      service_url.SchemeIsSecure(),
      new HttpBridgeFactory(base_request_context_),
      new HttpBridgeFactory(base_request_context_),
      bookmark_model_worker,
      attempt_last_user_authentication,
      MakeUserAgentForSyncapi().c_str());
  DCHECK(success) << "Syncapi initialization failed!";
}

void SyncBackendHost::Core::DoAuthenticate(const std::string& username,
                                           const std::string& password) {
  DCHECK(MessageLoop::current() == host_->core_thread_.message_loop());
  syncapi_->Authenticate(username.c_str(), password.c_str());
}

void SyncBackendHost::Core::DoShutdown(bool sync_disabled) {
  DCHECK(MessageLoop::current() == host_->core_thread_.message_loop());

  save_changes_timer_.Stop();
  syncapi_->Shutdown();  // Stops the SyncerThread.
  syncapi_->RemoveObserver();
  host_->bookmark_model_worker_->OnSyncerShutdownComplete();

  if (sync_disabled &&
      file_util::DirectoryExists(host_->sync_data_folder_path())) {
    // Delete the sync data folder to cleanup backend data.
    bool success = file_util::Delete(host_->sync_data_folder_path(), true);
    DCHECK(success);
  }

  host_ = NULL;
}

static AuthErrorState AuthProblemToAuthError(
    const sync_api::SyncManager::AuthProblem& auth_problem) {
  switch (auth_problem) {
    case sync_api::SyncManager::AUTH_PROBLEM_NONE:
      return AUTH_ERROR_NONE;
    case sync_api::SyncManager::AUTH_PROBLEM_INVALID_GAIA_CREDENTIALS:
      return AUTH_ERROR_INVALID_GAIA_CREDENTIALS;
    case sync_api::SyncManager::AUTH_PROBLEM_CONNECTION_FAILED:
      return AUTH_ERROR_CONNECTION_FAILED;
    case sync_api::SyncManager::AUTH_PROBLEM_USER_NOT_SIGNED_UP:
      return AUTH_ERROR_USER_NOT_SIGNED_UP;
  }

  NOTREACHED() << "Unknown AuthProblem.";
  return AUTH_ERROR_NONE;
}

void SyncBackendHost::Core::OnChangesApplied(
    const sync_api::BaseTransaction* trans,
    const sync_api::SyncManager::ChangeRecord* changes,
    int change_count) {
  if (!host_ || !host_->frontend_) {
    DCHECK(false) << "OnChangesApplied called after Shutdown?";
    return;
  }

  // ChangesApplied is the one exception that should come over from the sync
  // backend already on the service_loop_ thanks to our BookmarkModelWorker.
  // SyncFrontend changes exclusively on the UI loop, because it updates
  // the bookmark model.  As such, we don't need to worry about changes that
  // have been made to the bookmark model but not yet applied to the sync
  // model -- such changes only happen on the UI loop, and there's no
  // contention.
  if (host_->frontend_loop_ != MessageLoop::current()) {
    // TODO(ncarter): Bug 1480644.  Make this a DCHECK once syncapi filters
    // out all irrelevant changes.
    DLOG(WARNING) << "Could not update bookmark model from non-UI thread";
    return;
  }
  host_->processor_->ApplyChangesFromSyncModel(trans, changes, change_count);
}

void SyncBackendHost::Core::OnSyncCycleCompleted() {
  host_->frontend_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
      &Core::NotifyFrontend, SYNC_CYCLE_COMPLETED));
}

void SyncBackendHost::Core::OnInitializationComplete() {
  if (!host_ || !host_->frontend_)
    return;  // We may have been told to Shutdown before initialization
             // completed.

  // We could be on some random sync backend thread, so MessageLoop::current()
  // can definitely be null in here.
  host_->frontend_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &Core::NotifyFrontend, INITIALIZED));

  // Initialization is complete, so we can schedule recurring SaveChanges.
  host_->core_thread_.message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &Core::StartSavingChanges));
}

void SyncBackendHost::Core::OnAuthProblem(
    sync_api::SyncManager::AuthProblem auth_problem) {
  // We could be on SyncEngine_AuthWatcherThread.  Post to our core loop so
  // we can modify state.
  host_->frontend_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &Core::HandleAuthErrorEventOnFrontendLoop,
      AuthProblemToAuthError(auth_problem)));
}

void SyncBackendHost::Core::HandleAuthErrorEventOnFrontendLoop(
    AuthErrorState new_auth_error) {
  if (!host_ || !host_->frontend_)
    return;

  DCHECK_EQ(MessageLoop::current(), host_->frontend_loop_);

  host_->last_auth_error_ = new_auth_error;
  host_->frontend_->OnAuthError();
}

void SyncBackendHost::Core::StartSavingChanges() {
  save_changes_timer_.Start(
      base::TimeDelta::FromSeconds(kSaveChangesIntervalSeconds),
      this, &Core::SaveChanges);
}

void SyncBackendHost::Core::SaveChanges() {
  syncapi_->SaveChanges();
}

}  // namespace browser_sync

#endif
