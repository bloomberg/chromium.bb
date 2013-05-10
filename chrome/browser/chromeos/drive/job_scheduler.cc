// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/job_scheduler.h"

#include <math.h>

#include "base/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/logging.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {

namespace {
const int kMaxThrottleCount = 5;
}

const int JobScheduler::kMaxJobCount[] = {
  5,  // METADATA_QUEUE
  1,  // FILE_QUEUE
};

JobScheduler::JobEntry::JobEntry(JobType type)
    : job_info(type),
      context(DriveClientContext(USER_INITIATED)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

JobScheduler::JobEntry::~JobEntry() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

bool JobScheduler::JobEntry::Less(const JobEntry& left, const JobEntry& right) {
  // Lower values of ContextType are higher priority.
  // See also the comment at ContextType.
  return (left.context.type < right.context.type);
}

JobScheduler::JobScheduler(
    Profile* profile,
    google_apis::DriveServiceInterface* drive_service)
    : throttle_count_(0),
      disable_throttling_(false),
      drive_service_(drive_service),
      uploader_(new google_apis::DriveUploader(drive_service)),
      profile_(profile),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (int i = 0; i < NUM_QUEUES; ++i) {
    jobs_running_[i] = 0;
  }

  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
}

JobScheduler::~JobScheduler() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  size_t num_pending_jobs = 0;
  size_t num_running_jobs = 0;
  for (int i = 0; i < NUM_QUEUES; ++i) {
    num_pending_jobs += queue_[i].size();
    num_running_jobs += jobs_running_[i];
  }
  DCHECK_EQ(num_pending_jobs + num_running_jobs, job_map_.size());

  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

std::vector<JobInfo> JobScheduler::GetJobInfoList() {
  std::vector<JobInfo> job_info_list;
  for (JobIDMap::iterator iter(&job_map_); !iter.IsAtEnd(); iter.Advance())
    job_info_list.push_back(iter.GetCurrentValue()->job_info);
  return job_info_list;
}

void JobScheduler::AddObserver(JobListObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observer_list_.AddObserver(observer);
}

void JobScheduler::RemoveObserver(JobListObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observer_list_.RemoveObserver(observer);
}

void JobScheduler::CancelJob(JobID job_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(kinaba): Move the cancellation feature from DriveService
  // to JobScheduler. In particular, implement cancel based on job_id.
  // crbug.com/231029
  JobEntry* job = job_map_.Lookup(job_id);
  if (job)
    drive_service_->CancelForFilePath(job->job_info.file_path);
}

void JobScheduler::CancelAllJobs() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(kinaba): Move the cancellation feature from DriveService
  // to JobScheduler.
  drive_service_->CancelAll();
}

void JobScheduler::GetAccountMetadata(
    const google_apis::GetAccountMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<JobEntry> new_job(new JobEntry(TYPE_GET_ACCOUNT_METADATA));
  new_job->get_account_metadata_callback = callback;

  StartNewJob(new_job.Pass());
}

void JobScheduler::GetAboutResource(
    const google_apis::GetAboutResourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<JobEntry> new_job(new JobEntry(TYPE_GET_ABOUT_RESOURCE));
  new_job->get_about_resource_callback = callback;

  StartNewJob(new_job.Pass());
}

void JobScheduler::GetAppList(
    const google_apis::GetAppListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<JobEntry> new_job(new JobEntry(TYPE_GET_APP_LIST));
  new_job->get_app_list_callback = callback;

  StartNewJob(new_job.Pass());
}

void JobScheduler::GetAllResourceList(
    const google_apis::GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<JobEntry> new_job(new JobEntry(TYPE_GET_ALL_RESOURCE_LIST));
  new_job->get_resource_list_callback = callback;

  StartNewJob(new_job.Pass());
}

void JobScheduler::GetResourceListInDirectory(
    const std::string& directory_resource_id,
    const google_apis::GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<JobEntry> new_job(new JobEntry(
      TYPE_GET_RESOURCE_LIST_IN_DIRECTORY));
  new_job->directory_resource_id = directory_resource_id;
  new_job->get_resource_list_callback = callback;

  StartNewJob(new_job.Pass());
}

void JobScheduler::Search(
    const std::string& search_query,
    const google_apis::GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<JobEntry> new_job(new JobEntry(TYPE_SEARCH));
  new_job->search_query = search_query;
  new_job->get_resource_list_callback = callback;

  StartNewJob(new_job.Pass());
}

void JobScheduler::GetChangeList(
    int64 start_changestamp,
    const google_apis::GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<JobEntry> new_job(new JobEntry(TYPE_GET_CHANGE_LIST));
  new_job->start_changestamp = start_changestamp;
  new_job->get_resource_list_callback = callback;

  StartNewJob(new_job.Pass());
}

void JobScheduler::ContinueGetResourceList(
    const GURL& feed_url,
    const google_apis::GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<JobEntry> new_job(new JobEntry(TYPE_CONTINUE_GET_RESOURCE_LIST));
  new_job->feed_url = feed_url;
  new_job->get_resource_list_callback = callback;

  StartNewJob(new_job.Pass());
}

void JobScheduler::GetResourceEntry(
    const std::string& resource_id,
    const DriveClientContext& context,
    const google_apis::GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<JobEntry> new_job(new JobEntry(TYPE_GET_RESOURCE_ENTRY));
  new_job->resource_id = resource_id;
  new_job->context = context;
  new_job->get_resource_entry_callback = callback;

  StartNewJob(new_job.Pass());
}

void JobScheduler::DeleteResource(
    const std::string& resource_id,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<JobEntry> new_job(new JobEntry(TYPE_DELETE_RESOURCE));
  new_job->resource_id = resource_id;
  new_job->entry_action_callback = callback;

  StartNewJob(new_job.Pass());
}


void JobScheduler::CopyHostedDocument(
    const std::string& resource_id,
    const std::string& new_name,
    const google_apis::GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<JobEntry> new_job(new JobEntry(TYPE_COPY_HOSTED_DOCUMENT));
  new_job->resource_id = resource_id;
  new_job->new_name = new_name;
  new_job->get_resource_entry_callback = callback;

  StartNewJob(new_job.Pass());
}

void JobScheduler::RenameResource(
    const std::string& resource_id,
    const std::string& new_name,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<JobEntry> new_job(new JobEntry(TYPE_RENAME_RESOURCE));
  new_job->resource_id = resource_id;
  new_job->new_name = new_name;
  new_job->entry_action_callback = callback;

  StartNewJob(new_job.Pass());
}

void JobScheduler::AddResourceToDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<JobEntry> new_job(new JobEntry(TYPE_ADD_RESOURCE_TO_DIRECTORY));
  new_job->parent_resource_id = parent_resource_id;
  new_job->resource_id = resource_id;
  new_job->entry_action_callback = callback;

  StartNewJob(new_job.Pass());
}

void JobScheduler::RemoveResourceFromDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<JobEntry> new_job(new JobEntry(
      TYPE_REMOVE_RESOURCE_FROM_DIRECTORY));
  new_job->parent_resource_id = parent_resource_id;
  new_job->resource_id = resource_id;
  new_job->entry_action_callback = callback;

  StartNewJob(new_job.Pass());
}

void JobScheduler::AddNewDirectory(
    const std::string& parent_resource_id,
    const std::string& directory_name,
    const google_apis::GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<JobEntry> new_job(new JobEntry(TYPE_ADD_NEW_DIRECTORY));
  new_job->parent_resource_id = parent_resource_id;
  new_job->directory_name = directory_name;
  new_job->get_resource_entry_callback = callback;

  StartNewJob(new_job.Pass());
}

JobID JobScheduler::DownloadFile(
    const base::FilePath& virtual_path,
    const base::FilePath& local_cache_path,
    const GURL& download_url,
    const DriveClientContext& context,
    const google_apis::DownloadActionCallback& download_action_callback,
    const google_apis::GetContentCallback& get_content_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<JobEntry> new_job(new JobEntry(TYPE_DOWNLOAD_FILE));
  new_job->drive_file_path = virtual_path;
  new_job->local_file_path = local_cache_path;
  new_job->download_url = download_url;
  new_job->context = context;
  new_job->download_action_callback = download_action_callback;
  new_job->get_content_callback = get_content_callback;

  return StartNewJob(new_job.Pass());
}

void JobScheduler::UploadNewFile(
    const std::string& parent_resource_id,
    const base::FilePath& drive_file_path,
    const base::FilePath& local_file_path,
    const std::string& title,
    const std::string& content_type,
    const DriveClientContext& context,
    const google_apis::UploadCompletionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<JobEntry> new_job(new JobEntry(TYPE_UPLOAD_NEW_FILE));
  new_job->resource_id = parent_resource_id;
  new_job->drive_file_path = drive_file_path;
  new_job->local_file_path = local_file_path;
  new_job->title = title;
  new_job->content_type = content_type;
  new_job->upload_completion_callback = callback;
  new_job->context = context;

  StartNewJob(new_job.Pass());
}

void JobScheduler::UploadExistingFile(
    const std::string& resource_id,
    const base::FilePath& drive_file_path,
    const base::FilePath& local_file_path,
    const std::string& content_type,
    const std::string& etag,
    const DriveClientContext& context,
    const google_apis::UploadCompletionCallback& upload_completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<JobEntry> new_job(new JobEntry(TYPE_UPLOAD_EXISTING_FILE));
  new_job->resource_id = resource_id;
  new_job->drive_file_path = drive_file_path;
  new_job->local_file_path = local_file_path;
  new_job->content_type = content_type;
  new_job->etag = etag;
  new_job->upload_completion_callback = upload_completion_callback;
  new_job->context = context;

  StartNewJob(new_job.Pass());
}

void JobScheduler::CreateFile(
    const std::string& parent_resource_id,
    const base::FilePath& drive_file_path,
    const std::string& title,
    const std::string& content_type,
    const DriveClientContext& context,
    const google_apis::UploadCompletionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<JobEntry> new_job(new JobEntry(TYPE_CREATE_FILE));
  new_job->resource_id = parent_resource_id;
  new_job->drive_file_path = drive_file_path;
  new_job->title = title;
  new_job->content_type = content_type;
  new_job->upload_completion_callback = callback;
  new_job->context = context;

  StartNewJob(new_job.Pass());
}

JobID JobScheduler::StartNewJob(scoped_ptr<JobEntry> job) {
  DCHECK(job);

  // |job| is owned by job_map_ and released when it is removed in OnJobDone.
  JobInfo* job_info = &job->job_info;
  job_info->file_path = job->drive_file_path;
  job_info->job_id = job_map_.Add(job.release());

  QueueJob(job_info->job_id);
  NotifyJobAdded(*job_info);
  StartJobLoop(GetJobQueueType(job_info->job_type));
  return job_info->job_id;
}

void JobScheduler::QueueJob(JobID job_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  JobEntry* job_entry = job_map_.Lookup(job_id);
  DCHECK(job_entry);
  const JobInfo& job_info = job_entry->job_info;

  QueueType queue_type = GetJobQueueType(job_info.job_type);
  std::list<JobID>* queue = &queue_[queue_type];

  std::list<JobID>::iterator it = queue->begin();
  for (; it != queue->end(); ++it) {
    JobEntry* job_entry2 = job_map_.Lookup(*it);
    if (JobEntry::Less(*job_entry, *job_entry2))
      break;
  }
  queue->insert(it, job_id);

  util::Log("Job queued: %s - %s", job_info.ToString().c_str(),
            GetQueueInfo(queue_type).c_str());
}

void JobScheduler::StartJobLoop(QueueType queue_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (jobs_running_[queue_type] < kMaxJobCount[queue_type])
    DoJobLoop(queue_type);
}

void JobScheduler::DoJobLoop(QueueType queue_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (queue_[queue_type].empty()) {
    return;
  }

  JobID job_id = queue_[queue_type].front();
  JobEntry* entry = job_map_.Lookup(job_id);
  DCHECK(entry);

  // Check if we should defer based on the first item in the queue
  if (ShouldStopJobLoop(queue_type, entry->context)) {
    return;
  }

  // Increment the number of jobs.
  ++jobs_running_[queue_type];

  queue_[queue_type].pop_front();

  JobInfo* job_info = &entry->job_info;
  job_info->state = STATE_RUNNING;
  job_info->start_time = base::Time::Now();
  NotifyJobUpdated(*job_info);

  switch (job_info->job_type) {
    case TYPE_GET_ABOUT_RESOURCE: {
      drive_service_->GetAboutResource(
          base::Bind(&JobScheduler::OnGetAboutResourceJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id,
                     entry->get_about_resource_callback));
    }
    break;

    case TYPE_GET_ACCOUNT_METADATA: {
      drive_service_->GetAccountMetadata(
          base::Bind(&JobScheduler::OnGetAccountMetadataJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id,
                     entry->get_account_metadata_callback));
    }
    break;

    case TYPE_GET_APP_LIST: {
      drive_service_->GetAppList(
          base::Bind(&JobScheduler::OnGetAppListJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id,
                     entry->get_app_list_callback));
    }
    break;

    case TYPE_GET_ALL_RESOURCE_LIST: {
      drive_service_->GetAllResourceList(
          base::Bind(&JobScheduler::OnGetResourceListJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id,
                     entry->get_resource_list_callback));
    }
    break;

    case TYPE_GET_RESOURCE_LIST_IN_DIRECTORY: {
      drive_service_->GetResourceListInDirectory(
          entry->directory_resource_id,
          base::Bind(&JobScheduler::OnGetResourceListJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id,
                     entry->get_resource_list_callback));
    }
    break;

    case TYPE_SEARCH: {
      drive_service_->Search(
          entry->search_query,
          base::Bind(&JobScheduler::OnGetResourceListJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id,
                     entry->get_resource_list_callback));
    }
    break;

    case TYPE_GET_CHANGE_LIST: {
      drive_service_->GetChangeList(
          entry->start_changestamp,
          base::Bind(&JobScheduler::OnGetResourceListJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id,
                     entry->get_resource_list_callback));
    }
    break;

    case TYPE_CONTINUE_GET_RESOURCE_LIST: {
      drive_service_->ContinueGetResourceList(
          entry->feed_url,
          base::Bind(&JobScheduler::OnGetResourceListJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id,
                     entry->get_resource_list_callback));
    }
    break;

    case TYPE_GET_RESOURCE_ENTRY: {
      drive_service_->GetResourceEntry(
          entry->resource_id,
          base::Bind(&JobScheduler::OnGetResourceEntryJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id,
                     entry->get_resource_entry_callback));
    }
    break;

    case TYPE_DELETE_RESOURCE: {
      drive_service_->DeleteResource(
          entry->resource_id,
          "",  // etag
          base::Bind(&JobScheduler::OnEntryActionJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id,
                     entry->entry_action_callback));
    }
    break;

    case TYPE_COPY_HOSTED_DOCUMENT: {
      drive_service_->CopyHostedDocument(
          entry->resource_id,
          entry->new_name,
          base::Bind(&JobScheduler::OnGetResourceEntryJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id,
                     entry->get_resource_entry_callback));
    }
    break;

    case TYPE_RENAME_RESOURCE: {
      drive_service_->RenameResource(
          entry->resource_id,
          entry->new_name,
          base::Bind(&JobScheduler::OnEntryActionJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id,
                     entry->entry_action_callback));
    }
    break;

    case TYPE_ADD_RESOURCE_TO_DIRECTORY: {
      drive_service_->AddResourceToDirectory(
          entry->parent_resource_id,
          entry->resource_id,
          base::Bind(&JobScheduler::OnEntryActionJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id,
                     entry->entry_action_callback));
    }
    break;

    case TYPE_REMOVE_RESOURCE_FROM_DIRECTORY: {
      drive_service_->RemoveResourceFromDirectory(
          entry->parent_resource_id,
          entry->resource_id,
          base::Bind(&JobScheduler::OnEntryActionJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id,
                     entry->entry_action_callback));
    }
    break;

    case TYPE_ADD_NEW_DIRECTORY: {
      drive_service_->AddNewDirectory(
          entry->parent_resource_id,
          entry->directory_name,
          base::Bind(&JobScheduler::OnGetResourceEntryJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id,
                     entry->get_resource_entry_callback));
    }
    break;

    case TYPE_DOWNLOAD_FILE: {
      drive_service_->DownloadFile(
          entry->drive_file_path,
          entry->local_file_path,
          entry->download_url,
          base::Bind(&JobScheduler::OnDownloadActionJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id,
                     entry->download_action_callback),
          entry->get_content_callback,
          base::Bind(&JobScheduler::UpdateProgress,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id));
    }
    break;

    case TYPE_UPLOAD_NEW_FILE: {
      uploader_->UploadNewFile(
          entry->resource_id,
          entry->drive_file_path,
          entry->local_file_path,
          entry->title,
          entry->content_type,
          base::Bind(&JobScheduler::OnUploadCompletionJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id,
                     entry->upload_completion_callback),
          base::Bind(&JobScheduler::UpdateProgress,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id));
    }
    break;

    case TYPE_UPLOAD_EXISTING_FILE: {
      uploader_->UploadExistingFile(
          entry->resource_id,
          entry->drive_file_path,
          entry->local_file_path,
          entry->content_type,
          entry->etag,
          base::Bind(&JobScheduler::OnUploadCompletionJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id,
                     entry->upload_completion_callback),
          base::Bind(&JobScheduler::UpdateProgress,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id));
    }
    break;

    case TYPE_CREATE_FILE: {
      // For now, creation is implemented by uploading an empty file /dev/null.
      // TODO(kinaba): http://crbug.com/135143. Implement in a nicer way.
      uploader_->UploadNewFile(
          entry->resource_id,
          entry->drive_file_path,
          base::FilePath(util::kSymLinkToDevNull),
          entry->title,
          entry->content_type,
          base::Bind(&JobScheduler::OnUploadCompletionJobDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     job_id,
                     entry->upload_completion_callback),
          google_apis::ProgressCallback());
    }
    break;

    // There is no default case so that there will be a compiler error if a type
    // is added but unhandled.
  }

  util::Log("Job started: %s - %s",
            job_info->ToString().c_str(),
            GetQueueInfo(queue_type).c_str());
}

bool JobScheduler::ShouldStopJobLoop(QueueType queue_type,
                                     const DriveClientContext& context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Should stop if the gdata feature was disabled while running the fetch
  // loop.
  if (profile_->GetPrefs()->GetBoolean(prefs::kDisableDrive))
    return true;

  // Should stop if the network is not online.
  if (net::NetworkChangeNotifier::IsOffline())
    return true;

  // Should stop background jobs if the current connection is on cellular
  // network, and fetching is disabled over cellular.
  bool should_stop_on_cellular_network = false;
  switch (context.type) {
    case USER_INITIATED:
      should_stop_on_cellular_network = false;
      break;
    case BACKGROUND:
      should_stop_on_cellular_network = (queue_type == FILE_QUEUE);
      break;
  }
  if (should_stop_on_cellular_network &&
      profile_->GetPrefs()->GetBoolean(prefs::kDisableDriveOverCellular) &&
      net::NetworkChangeNotifier::IsConnectionCellular(
          net::NetworkChangeNotifier::GetConnectionType())) {
    return true;
  }

  return false;
}

void JobScheduler::ThrottleAndContinueJobLoop(QueueType queue_type) {
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
      base::Bind(&JobScheduler::DoJobLoop,
                 weak_ptr_factory_.GetWeakPtr(),
                 queue_type),
      delay);
  DCHECK(posted);
}

void JobScheduler::ResetThrottleAndContinueJobLoop(QueueType queue_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Post a task to continue the job loop.  This allows us to finish handling
  // the current job before starting the next one.
  throttle_count_ = 0;
  base::MessageLoopProxy::current()->PostTask(FROM_HERE,
      base::Bind(&JobScheduler::DoJobLoop,
                 weak_ptr_factory_.GetWeakPtr(),
                 queue_type));
}

bool JobScheduler::OnJobDone(JobID job_id, FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  JobEntry* job_entry = job_map_.Lookup(job_id);
  DCHECK(job_entry);
  JobInfo* job_info = &job_entry->job_info;
  QueueType queue_type = GetJobQueueType(job_info->job_type);

  const base::TimeDelta elapsed = base::Time::Now() - job_info->start_time;
  util::Log("Job done: %s => %s (elapsed time: %sms) - %s",
            job_info->ToString().c_str(),
            FileErrorToString(error).c_str(),
            base::Int64ToString(elapsed.InMilliseconds()).c_str(),
            GetQueueInfo(queue_type).c_str());

  // Decrement the number of jobs for this queue.
  --jobs_running_[queue_type];

  // Retry, depending on the error.
  if (error == FILE_ERROR_THROTTLED) {
    job_info->state = STATE_RETRY;
    NotifyJobUpdated(*job_info);

    // Requeue the job.
    QueueJob(job_id);

    ThrottleAndContinueJobLoop(queue_type);
    return false;
  } else {
    NotifyJobDone(*job_info, error);
    // The job has finished, no retry will happen in the scheduler. Now we can
    // remove the job info from the map. This is the only place of the removal.
    job_map_.Remove(job_id);

    ResetThrottleAndContinueJobLoop(queue_type);
    return true;
  }
}

void JobScheduler::OnGetResourceListJobDone(
    JobID job_id,
    const google_apis::GetResourceListCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceList> resource_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (OnJobDone(job_id, util::GDataToFileError(error)))
    callback.Run(error, resource_list.Pass());
}

void JobScheduler::OnGetResourceEntryJobDone(
    JobID job_id,
    const google_apis::GetResourceEntryCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (OnJobDone(job_id, util::GDataToFileError(error)))
    callback.Run(error, entry.Pass());
}

void JobScheduler::OnGetAboutResourceJobDone(
    JobID job_id,
    const google_apis::GetAboutResourceCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::AboutResource> about_resource) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (OnJobDone(job_id, util::GDataToFileError(error)))
    callback.Run(error, about_resource.Pass());
}

void JobScheduler::OnGetAccountMetadataJobDone(
    JobID job_id,
    const google_apis::GetAccountMetadataCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::AccountMetadata> account_metadata) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (OnJobDone(job_id, util::GDataToFileError(error)))
    callback.Run(error, account_metadata.Pass());
}

void JobScheduler::OnGetAppListJobDone(
    JobID job_id,
    const google_apis::GetAppListCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::AppList> app_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (OnJobDone(job_id, util::GDataToFileError(error)))
    callback.Run(error, app_list.Pass());
}

void JobScheduler::OnEntryActionJobDone(
    JobID job_id,
    const google_apis::EntryActionCallback& callback,
    google_apis::GDataErrorCode error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (OnJobDone(job_id, util::GDataToFileError(error)))
    callback.Run(error);
}

void JobScheduler::OnDownloadActionJobDone(
    JobID job_id,
    const google_apis::DownloadActionCallback& callback,
    google_apis::GDataErrorCode error,
    const base::FilePath& temp_file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (OnJobDone(job_id, util::GDataToFileError(error)))
    callback.Run(error, temp_file);
}

void JobScheduler::OnUploadCompletionJobDone(
    JobID job_id,
    const google_apis::UploadCompletionCallback& callback,
    google_apis::GDataErrorCode error,
    const base::FilePath& drive_path,
    const base::FilePath& file_path,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (OnJobDone(job_id, util::GDataToFileError(error)))
    callback.Run(error, drive_path, file_path, resource_entry.Pass());
}

void JobScheduler::UpdateProgress(JobID job_id, int64 progress, int64 total) {
  JobEntry* job_entry = job_map_.Lookup(job_id);
  DCHECK(job_entry);

  job_entry->job_info.num_completed_bytes = progress;
  job_entry->job_info.num_total_bytes = total;
  NotifyJobUpdated(job_entry->job_info);
}

void JobScheduler::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Resume the job loop if the network is back online. Note that we don't
  // need to check the type of the network as it will be checked in
  // ShouldStopJobLoop() as soon as the loop is resumed.
  if (!net::NetworkChangeNotifier::IsOffline()) {
    for (int i = METADATA_QUEUE; i < NUM_QUEUES; ++i) {
      StartJobLoop(static_cast<QueueType>(i));
    }
  }
}

JobScheduler::QueueType JobScheduler::GetJobQueueType(JobType type) {
  switch (type) {
    case TYPE_GET_ABOUT_RESOURCE:
    case TYPE_GET_ACCOUNT_METADATA:
    case TYPE_GET_APP_LIST:
    case TYPE_GET_ALL_RESOURCE_LIST:
    case TYPE_GET_RESOURCE_LIST_IN_DIRECTORY:
    case TYPE_SEARCH:
    case TYPE_GET_CHANGE_LIST:
    case TYPE_CONTINUE_GET_RESOURCE_LIST:
    case TYPE_GET_RESOURCE_ENTRY:
    case TYPE_DELETE_RESOURCE:
    case TYPE_COPY_HOSTED_DOCUMENT:
    case TYPE_RENAME_RESOURCE:
    case TYPE_ADD_RESOURCE_TO_DIRECTORY:
    case TYPE_REMOVE_RESOURCE_FROM_DIRECTORY:
    case TYPE_ADD_NEW_DIRECTORY:
    case TYPE_CREATE_FILE:
      return METADATA_QUEUE;

    case TYPE_DOWNLOAD_FILE:
    case TYPE_UPLOAD_NEW_FILE:
    case TYPE_UPLOAD_EXISTING_FILE:
      return FILE_QUEUE;
  }
  NOTREACHED();
  return FILE_QUEUE;
}

void JobScheduler::NotifyJobAdded(const JobInfo& job_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(JobListObserver, observer_list_, OnJobAdded(job_info));
}

void JobScheduler::NotifyJobDone(const JobInfo& job_info,
                                   FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(JobListObserver, observer_list_,
                    OnJobDone(job_info, error));
}

void JobScheduler::NotifyJobUpdated(const JobInfo& job_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(JobListObserver, observer_list_, OnJobUpdated(job_info));
}

std::string JobScheduler::GetQueueInfo(QueueType type) const {
  return base::StringPrintf("%s: pending: %d, running: %d",
                            QueueTypeToString(type).c_str(),
                            static_cast<int>(queue_[type].size()),
                            jobs_running_[type]);
}

// static
std::string JobScheduler::QueueTypeToString(QueueType type) {
  switch (type) {
    case METADATA_QUEUE:
      return "METADATA_QUEUE";
    case FILE_QUEUE:
      return "FILE_QUEUE";
    case NUM_QUEUES:
      return "NUM_QUEUES";
  }
  NOTREACHED();
  return "";
}


}  // namespace drive
