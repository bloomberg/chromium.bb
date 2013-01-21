// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_scheduler.h"

#include <math.h>

#include "base/message_loop.h"
#include "base/rand_util.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {

namespace {
const int kMaxThrottleCount = 5;
}

DriveScheduler::JobInfo::JobInfo(JobType in_job_type)
    : job_type(in_job_type),
      job_id(-1),
      completed_bytes(0),
      total_bytes(0),
      state(STATE_NONE) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

DriveScheduler::QueueEntry::QueueEntry(JobType in_job_type)
    : job_info(in_job_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

DriveScheduler::QueueEntry::~QueueEntry() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

DriveScheduler::DriveScheduler(
    Profile* profile,
    google_apis::DriveServiceInterface* drive_service)
    : job_loop_is_running_(false),
      next_job_id_(0),
      throttle_count_(0),
      disable_throttling_(false),
      drive_service_(drive_service),
      profile_(profile),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      initialized_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

DriveScheduler::~DriveScheduler() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(initialized_);
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

void DriveScheduler::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Initialize() may be called more than once for the lifetime when the
  // file system is remounted.
  if (initialized_)
    return;

  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
  initialized_ = true;
}

void DriveScheduler::GetAccountMetadata(
    const google_apis::GetAccountMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<QueueEntry> new_job(new QueueEntry(TYPE_GET_ACCOUNT_METADATA));
  new_job->get_account_metadata_callback = callback;

  QueueJob(new_job.Pass());

  StartJobLoop();
}

void DriveScheduler::GetAppList(
    const google_apis::GetAppListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<QueueEntry> new_job(new QueueEntry(TYPE_GET_APP_LIST));
  new_job->get_app_list_callback = callback;

  QueueJob(new_job.Pass());

  StartJobLoop();
}

void DriveScheduler::GetResourceList(
    const GURL& feed_url,
    int64 start_changestamp,
    const std::string& search_query,
    bool shared_with_me,
    const std::string& directory_resource_id,
    const google_apis::GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<QueueEntry> new_job(new QueueEntry(TYPE_GET_RESOURCE_LIST));
  new_job->feed_url = feed_url;
  new_job->start_changestamp = start_changestamp;
  new_job->search_query = search_query;
  new_job->shared_with_me = shared_with_me;
  new_job->directory_resource_id = directory_resource_id;
  new_job->get_resource_list_callback = callback;

  QueueJob(new_job.Pass());

  StartJobLoop();
}

void DriveScheduler::GetResourceEntry(
    const std::string& resource_id,
    const google_apis::GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<QueueEntry> new_job(new QueueEntry(TYPE_GET_RESOURCE_ENTRY));
  new_job->resource_id = resource_id;
  new_job->get_resource_entry_callback = callback;

  QueueJob(new_job.Pass());

  StartJobLoop();
}

void DriveScheduler::DeleteResource(
    const GURL& edit_url,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<QueueEntry> new_job(new QueueEntry(TYPE_DELETE_RESOURCE));
  new_job->edit_url = edit_url;
  new_job->entry_action_callback = callback;

  QueueJob(new_job.Pass());

  StartJobLoop();
}


void DriveScheduler::CopyHostedDocument(
    const std::string& resource_id,
    const std::string& new_name,
    const google_apis::GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<QueueEntry> new_job(new QueueEntry(TYPE_COPY_HOSTED_DOCUMENT));
  new_job->resource_id = resource_id;
  new_job->new_name = new_name;
  new_job->get_resource_entry_callback = callback;

  QueueJob(new_job.Pass());

  StartJobLoop();
}

void DriveScheduler::RenameResource(
    const GURL& edit_url,
    const std::string& new_name,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<QueueEntry> new_job(new QueueEntry(TYPE_RENAME_RESOURCE));
  new_job->edit_url = edit_url;
  new_job->new_name = new_name;
  new_job->entry_action_callback = callback;

  QueueJob(new_job.Pass());

  StartJobLoop();
}

void DriveScheduler::AddResourceToDirectory(
    const GURL& parent_content_url,
    const GURL& edit_url,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<QueueEntry> new_job(
      new QueueEntry(TYPE_ADD_RESOURCE_TO_DIRECTORY));
  new_job->parent_content_url = parent_content_url;
  new_job->edit_url = edit_url;
  new_job->entry_action_callback = callback;

  QueueJob(new_job.Pass());

  StartJobLoop();
}

void DriveScheduler::RemoveResourceFromDirectory(
    const GURL& parent_content_url,
    const std::string& resource_id,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<QueueEntry> new_job(
      new QueueEntry(TYPE_REMOVE_RESOURCE_FROM_DIRECTORY));
  new_job->parent_content_url = parent_content_url;
  new_job->resource_id = resource_id;
  new_job->entry_action_callback = callback;

  QueueJob(new_job.Pass());

  StartJobLoop();
}

void DriveScheduler::AddNewDirectory(
    const GURL& parent_content_url,
    const std::string& directory_name,
    const google_apis::GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<QueueEntry> new_job(new QueueEntry(TYPE_ADD_NEW_DIRECTORY));
  new_job->parent_content_url = parent_content_url;
  new_job->directory_name = directory_name;
  new_job->get_resource_entry_callback = callback;

  QueueJob(new_job.Pass());

  StartJobLoop();
}

void DriveScheduler::DownloadFile(
    const FilePath& virtual_path,
    const FilePath& local_cache_path,
    const GURL& content_url,
    const google_apis::DownloadActionCallback& download_action_callback,
    const google_apis::GetContentCallback& get_content_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<QueueEntry> new_job(new QueueEntry(TYPE_DOWNLOAD_FILE));
  new_job->virtual_path = virtual_path;
  new_job->local_cache_path = local_cache_path;
  new_job->content_url = content_url;
  new_job->download_action_callback = download_action_callback;
  new_job->get_content_callback = get_content_callback;

  QueueJob(new_job.Pass());

  StartJobLoop();
}

int DriveScheduler::QueueJob(scoped_ptr<QueueEntry> job) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  int job_id = next_job_id_;
  job->job_info.job_id = job_id;
  next_job_id_++;

  queue_.push_back(job_id);

  DCHECK(job_info_map_.find(job_id) == job_info_map_.end());
  job_info_map_[job_id] = make_linked_ptr(job.release());

  return job_id;
}

void DriveScheduler::StartJobLoop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!job_loop_is_running_)
    DoJobLoop();
}

void DriveScheduler::DoJobLoop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (queue_.empty() || ShouldStopJobLoop()) {
    // Note that |queue_| is not cleared so the sync loop can resume.
    job_loop_is_running_ = false;
    return;
  }
  job_loop_is_running_ = true;

  // Should copy before calling queue_.pop_front().
  int job_id = queue_.front();
  queue_.pop_front();

  JobMap::iterator job_iter = job_info_map_.find(job_id);
  DCHECK(job_iter != job_info_map_.end());

  JobInfo& job_info = job_iter->second->job_info;
  job_info.state = STATE_RUNNING;
  const QueueEntry* queue_entry = job_iter->second.get();

  switch (job_info.job_type) {
    case TYPE_GET_ACCOUNT_METADATA: {
      drive_service_->GetAccountMetadata(
          base::Bind(&DriveScheduler::OnGetAccountMetadataJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id));
    }
    break;

    case TYPE_GET_APP_LIST: {
      drive_service_->GetAppList(
          base::Bind(&DriveScheduler::OnGetAppListJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id));
    }
    break;

    case TYPE_GET_RESOURCE_LIST: {
      drive_service_->GetResourceList(
          queue_entry->feed_url,
          queue_entry->start_changestamp,
          queue_entry->search_query,
          queue_entry->shared_with_me,
          queue_entry->directory_resource_id,
          base::Bind(&DriveScheduler::OnGetResourceListJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id));
    }
    break;

    case TYPE_GET_RESOURCE_ENTRY: {
      drive_service_->GetResourceEntry(
          queue_entry->resource_id,
          base::Bind(&DriveScheduler::OnGetResourceEntryJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id));
    }
    break;

    case TYPE_DELETE_RESOURCE: {
      drive_service_->DeleteResource(
          queue_entry->edit_url,
          base::Bind(&DriveScheduler::OnEntryActionJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id));
    }
    break;


    case TYPE_COPY_HOSTED_DOCUMENT: {
      drive_service_->CopyHostedDocument(
          queue_entry->resource_id,
          queue_entry->new_name,
          base::Bind(&DriveScheduler::OnGetResourceEntryJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id));
    }
    break;

    case TYPE_RENAME_RESOURCE: {
      drive_service_->RenameResource(
          queue_entry->edit_url,
          queue_entry->new_name,
          base::Bind(&DriveScheduler::OnEntryActionJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id));
    }
    break;

    case TYPE_ADD_RESOURCE_TO_DIRECTORY: {
      drive_service_->AddResourceToDirectory(
          queue_entry->parent_content_url,
          queue_entry->edit_url,
          base::Bind(&DriveScheduler::OnEntryActionJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id));
    }
    break;

    case TYPE_REMOVE_RESOURCE_FROM_DIRECTORY: {
      drive_service_->RemoveResourceFromDirectory(
          queue_entry->parent_content_url,
          queue_entry->resource_id,
          base::Bind(&DriveScheduler::OnEntryActionJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id));
    }
    break;

    case TYPE_ADD_NEW_DIRECTORY: {
      drive_service_->AddNewDirectory(
          queue_entry->parent_content_url,
          queue_entry->directory_name,
          base::Bind(&DriveScheduler::OnGetResourceEntryJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id));
    }
    break;

    case TYPE_DOWNLOAD_FILE: {
      drive_service_->DownloadFile(
          queue_entry->virtual_path,
          queue_entry->local_cache_path,
          queue_entry->content_url,
          base::Bind(&DriveScheduler::OnDownloadActionJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id),
          queue_entry->get_content_callback);
    }
    break;

    // There is no default case so that there will be a compiler error if a type
    // is added but unhandled.
  }
}

bool DriveScheduler::ShouldStopJobLoop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Should stop if the gdata feature was disabled while running the fetch
  // loop.
  if (profile_->GetPrefs()->GetBoolean(prefs::kDisableDrive))
    return true;

  // Should stop if the network is not online.
  if (net::NetworkChangeNotifier::IsOffline())
    return true;

  // Should stop if the current connection is on cellular network, and
  // fetching is disabled over cellular.
  if (profile_->GetPrefs()->GetBoolean(prefs::kDisableDriveOverCellular) &&
      net::NetworkChangeNotifier::IsConnectionCellular(
          net::NetworkChangeNotifier::GetConnectionType()))
    return true;

  return false;
}

void DriveScheduler::ThrottleAndContinueJobLoop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (throttle_count_ < kMaxThrottleCount)
    throttle_count_++;

  base::TimeDelta delay;
  if (disable_throttling_) {
    delay = base::TimeDelta::FromSeconds(0);
  } else {
    delay =
      base::TimeDelta::FromSeconds(pow(2, throttle_count_ - 1)) +
      base::TimeDelta::FromMilliseconds(base::RandInt(0, 1000));
  }
  VLOG(1) << "Throttling for " << delay.InMillisecondsF();

  const bool posted = base::MessageLoopProxy::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DriveScheduler::DoJobLoop,
                 weak_ptr_factory_.GetWeakPtr()),
      delay);
  DCHECK(posted);
}

void DriveScheduler::ResetThrottleAndContinueJobLoop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Post a task to continue the job loop.  This allows us to finish handling
  // the current job before starting the next one.
  throttle_count_ = 0;
  base::MessageLoopProxy::current()->PostTask(FROM_HERE,
      base::Bind(&DriveScheduler::DoJobLoop,
                 weak_ptr_factory_.GetWeakPtr()));
}

scoped_ptr<DriveScheduler::QueueEntry> DriveScheduler::OnJobDone(
    int job_id,
    DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  JobMap::iterator job_iter = job_info_map_.find(job_id);
  DCHECK(job_iter != job_info_map_.end());

  // Retry, depending on the error.
  if (error == DRIVE_FILE_ERROR_THROTTLED) {
    job_iter->second->job_info.state = STATE_RETRY;

    // Requeue the job.
    queue_.push_back(job_id);
    ThrottleAndContinueJobLoop();

    return scoped_ptr<DriveScheduler::QueueEntry>();
  } else {
    scoped_ptr<DriveScheduler::QueueEntry> job_info(job_iter->second.release());

    // Delete the job.
    job_info_map_.erase(job_id);
    ResetThrottleAndContinueJobLoop();

    return job_info.Pass();
  }
}

void DriveScheduler::OnGetResourceListJobDone(
    int job_id,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceList> resource_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DriveFileError drive_error(util::GDataToDriveFileError(error));

  scoped_ptr<QueueEntry> job_info = OnJobDone(job_id, drive_error);

  if (!job_info)
    return;

  // Handle the callback.
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(job_info->get_resource_list_callback,
                 error,
                 base::Passed(&resource_list)));
}

void DriveScheduler::OnGetResourceEntryJobDone(
      int job_id,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DriveFileError drive_error(util::GDataToDriveFileError(error));

  scoped_ptr<QueueEntry> job_info = OnJobDone(job_id, drive_error);

  if (!job_info)
    return;

  // Handle the callback.
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(job_info->get_resource_entry_callback,
                 error,
                 base::Passed(&entry)));
}

void DriveScheduler::OnGetAccountMetadataJobDone(
    int job_id,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::AccountMetadataFeed> account_metadata) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DriveFileError drive_error(util::GDataToDriveFileError(error));

  scoped_ptr<QueueEntry> job_info = OnJobDone(job_id, drive_error);

  if (!job_info)
    return;

  // Handle the callback.
  job_info->get_account_metadata_callback.Run(error, account_metadata.Pass());
}

void DriveScheduler::OnGetAppListJobDone(
    int job_id,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::AppList> app_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DriveFileError drive_error(util::GDataToDriveFileError(error));

  scoped_ptr<QueueEntry> job_info = OnJobDone(job_id, drive_error);

  if (!job_info)
    return;

  // Handle the callback.
  job_info->get_app_list_callback.Run(error, app_list.Pass());
}

void DriveScheduler::OnEntryActionJobDone(int job_id,
                                          google_apis::GDataErrorCode error) {
  DriveFileError drive_error(util::GDataToDriveFileError(error));

  scoped_ptr<QueueEntry> job_info = OnJobDone(job_id, drive_error);

  if (!job_info)
    return;

  // Handle the callback.
  DCHECK(!job_info->entry_action_callback.is_null());
  job_info->entry_action_callback.Run(error);
}

void DriveScheduler::OnDownloadActionJobDone(int job_id,
                                             google_apis::GDataErrorCode error,
                                             const FilePath& temp_file) {
  DriveFileError drive_error(util::GDataToDriveFileError(error));

  scoped_ptr<QueueEntry> job_info = OnJobDone(job_id, drive_error);

  if (!job_info)
    return;

  // Handle the callback.
  DCHECK(!job_info->download_action_callback.is_null());
  job_info->download_action_callback.Run(error, temp_file);
}

void DriveScheduler::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Resume the job loop if the network is back online. Note that we don't
  // need to check the type of the network as it will be checked in
  // ShouldStopJobLoop() as soon as the loop is resumed.
  if (!net::NetworkChangeNotifier::IsOffline())
    StartJobLoop();
}

}  // namespace drive
