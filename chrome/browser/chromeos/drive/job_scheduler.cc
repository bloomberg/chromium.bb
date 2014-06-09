// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/job_scheduler.h"

#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/drive/event_logger.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/drive/drive_api_parser.h"

using content::BrowserThread;

namespace drive {

namespace {

// All jobs are retried at maximum of kMaxRetryCount when they fail due to
// throttling or server error.  The delay before retrying a job is shared among
// jobs. It doubles in length on each failure, upto 2^kMaxThrottleCount seconds.
//
// According to the API documentation, kMaxRetryCount should be the same as
// kMaxThrottleCount (https://developers.google.com/drive/handle-errors).
// But currently multiplied by 2 to ensure upload related jobs retried for a
// sufficient number of times. crbug.com/269918
const int kMaxThrottleCount = 4;
const int kMaxRetryCount = 2 * kMaxThrottleCount;

// GetDefaultValue returns a value constructed by the default constructor.
template<typename T> struct DefaultValueCreator {
  static T GetDefaultValue() { return T(); }
};
template<typename T> struct DefaultValueCreator<const T&> {
  static T GetDefaultValue() { return T(); }
};

// Helper of CreateErrorRunCallback implementation.
// Provides:
// - ResultType; the type of the Callback which should be returned by
//     CreateErrorRunCallback.
// - Run(): a static function which takes the original |callback| and |error|,
//     and runs the |callback|.Run() with the error code and default values
//     for remaining arguments.
template<typename CallbackType> struct CreateErrorRunCallbackHelper;

// CreateErrorRunCallback with two arguments.
template<typename P1>
struct CreateErrorRunCallbackHelper<void(google_apis::GDataErrorCode, P1)> {
  static void Run(
      const base::Callback<void(google_apis::GDataErrorCode, P1)>& callback,
      google_apis::GDataErrorCode error) {
    callback.Run(error, DefaultValueCreator<P1>::GetDefaultValue());
  }
};

// Returns a callback with the tail parameter bound to its default value.
// In other words, returned_callback.Run(error) runs callback.Run(error, T()).
template<typename CallbackType>
base::Callback<void(google_apis::GDataErrorCode)>
CreateErrorRunCallback(const base::Callback<CallbackType>& callback) {
  return base::Bind(&CreateErrorRunCallbackHelper<CallbackType>::Run, callback);
}

// Parameter struct for RunUploadNewFile.
struct UploadNewFileParams {
  std::string parent_resource_id;
  base::FilePath local_file_path;
  std::string title;
  std::string content_type;
  DriveUploader::UploadNewFileOptions options;
  UploadCompletionCallback callback;
  google_apis::ProgressCallback progress_callback;
};

// Helper function to work around the arity limitation of base::Bind.
google_apis::CancelCallback RunUploadNewFile(
    DriveUploaderInterface* uploader,
    const UploadNewFileParams& params) {
  return uploader->UploadNewFile(params.parent_resource_id,
                                 params.local_file_path,
                                 params.title,
                                 params.content_type,
                                 params.options,
                                 params.callback,
                                 params.progress_callback);
}

// Parameter struct for RunUploadExistingFile.
struct UploadExistingFileParams {
  std::string resource_id;
  base::FilePath local_file_path;
  std::string content_type;
  DriveUploader::UploadExistingFileOptions options;
  std::string etag;
  UploadCompletionCallback callback;
  google_apis::ProgressCallback progress_callback;
};

// Helper function to work around the arity limitation of base::Bind.
google_apis::CancelCallback RunUploadExistingFile(
    DriveUploaderInterface* uploader,
    const UploadExistingFileParams& params) {
  return uploader->UploadExistingFile(params.resource_id,
                                      params.local_file_path,
                                      params.content_type,
                                      params.options,
                                      params.callback,
                                      params.progress_callback);
}

// Parameter struct for RunResumeUploadFile.
struct ResumeUploadFileParams {
  GURL upload_location;
  base::FilePath local_file_path;
  std::string content_type;
  UploadCompletionCallback callback;
  google_apis::ProgressCallback progress_callback;
};

// Helper function to adjust the return type.
google_apis::CancelCallback RunResumeUploadFile(
    DriveUploaderInterface* uploader,
    const ResumeUploadFileParams& params) {
  return uploader->ResumeUploadFile(params.upload_location,
                                    params.local_file_path,
                                    params.content_type,
                                    params.callback,
                                    params.progress_callback);
}

}  // namespace

// Metadata jobs are cheap, so we run them concurrently. File jobs run serially.
const int JobScheduler::kMaxJobCount[] = {
  5,  // METADATA_QUEUE
  1,  // FILE_QUEUE
};

JobScheduler::JobEntry::JobEntry(JobType type)
    : job_info(type),
      context(ClientContext(USER_INITIATED)),
      retry_count(0) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

JobScheduler::JobEntry::~JobEntry() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

struct JobScheduler::ResumeUploadParams {
  base::FilePath drive_file_path;
  base::FilePath local_file_path;
  std::string content_type;
};

JobScheduler::JobScheduler(
    PrefService* pref_service,
    EventLogger* logger,
    DriveServiceInterface* drive_service,
    base::SequencedTaskRunner* blocking_task_runner)
    : throttle_count_(0),
      wait_until_(base::Time::Now()),
      disable_throttling_(false),
      logger_(logger),
      drive_service_(drive_service),
      uploader_(new DriveUploader(drive_service, blocking_task_runner)),
      pref_service_(pref_service),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (int i = 0; i < NUM_QUEUES; ++i)
    queue_[i].reset(new JobQueue(kMaxJobCount[i], NUM_CONTEXT_TYPES));

  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
}

JobScheduler::~JobScheduler() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  size_t num_queued_jobs = 0;
  for (int i = 0; i < NUM_QUEUES; ++i)
    num_queued_jobs += queue_[i]->GetNumberOfJobs();
  DCHECK_EQ(num_queued_jobs, job_map_.size());

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

  JobEntry* job = job_map_.Lookup(job_id);
  if (job) {
    if (job->job_info.state == STATE_RUNNING) {
      // If the job is running an HTTP request, cancel it via |cancel_callback|
      // returned from the request, and wait for termination in the normal
      // callback handler, OnJobDone.
      if (!job->cancel_callback.is_null())
        job->cancel_callback.Run();
    } else {
      AbortNotRunningJob(job, google_apis::GDATA_CANCELLED);
    }
  }
}

void JobScheduler::CancelAllJobs() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // CancelJob may remove the entry from |job_map_|. That's OK. IDMap supports
  // removable during iteration.
  for (JobIDMap::iterator iter(&job_map_); !iter.IsAtEnd(); iter.Advance())
    CancelJob(iter.GetCurrentKey());
}

void JobScheduler::GetAboutResource(
    const google_apis::AboutResourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  JobEntry* new_job = CreateNewJob(TYPE_GET_ABOUT_RESOURCE);
  new_job->task = base::Bind(
      &DriveServiceInterface::GetAboutResource,
      base::Unretained(drive_service_),
      base::Bind(&JobScheduler::OnGetAboutResourceJobDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 new_job->job_info.job_id,
                 callback));
  new_job->abort_callback = CreateErrorRunCallback(callback);
  StartJob(new_job);
}

void JobScheduler::GetAppList(const google_apis::AppListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  JobEntry* new_job = CreateNewJob(TYPE_GET_APP_LIST);
  new_job->task = base::Bind(
      &DriveServiceInterface::GetAppList,
      base::Unretained(drive_service_),
      base::Bind(&JobScheduler::OnGetAppListJobDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 new_job->job_info.job_id,
                 callback));
  new_job->abort_callback = CreateErrorRunCallback(callback);
  StartJob(new_job);
}

void JobScheduler::GetAllFileList(
    const google_apis::FileListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  JobEntry* new_job = CreateNewJob(TYPE_GET_ALL_RESOURCE_LIST);
  new_job->task = base::Bind(
      &DriveServiceInterface::GetAllFileList,
      base::Unretained(drive_service_),
      base::Bind(&JobScheduler::OnGetFileListJobDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 new_job->job_info.job_id,
                 callback));
  new_job->abort_callback = CreateErrorRunCallback(callback);
  StartJob(new_job);
}

void JobScheduler::GetFileListInDirectory(
    const std::string& directory_resource_id,
    const google_apis::FileListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  JobEntry* new_job = CreateNewJob(
      TYPE_GET_RESOURCE_LIST_IN_DIRECTORY);
  new_job->task = base::Bind(
      &DriveServiceInterface::GetFileListInDirectory,
      base::Unretained(drive_service_),
      directory_resource_id,
      base::Bind(&JobScheduler::OnGetFileListJobDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 new_job->job_info.job_id,
                 callback));
  new_job->abort_callback = CreateErrorRunCallback(callback);
  StartJob(new_job);
}

void JobScheduler::Search(const std::string& search_query,
                          const google_apis::FileListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  JobEntry* new_job = CreateNewJob(TYPE_SEARCH);
  new_job->task = base::Bind(
      &DriveServiceInterface::Search,
      base::Unretained(drive_service_),
      search_query,
      base::Bind(&JobScheduler::OnGetFileListJobDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 new_job->job_info.job_id,
                 callback));
  new_job->abort_callback = CreateErrorRunCallback(callback);
  StartJob(new_job);
}

void JobScheduler::GetChangeList(
    int64 start_changestamp,
    const google_apis::ChangeListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  JobEntry* new_job = CreateNewJob(TYPE_GET_CHANGE_LIST);
  new_job->task = base::Bind(
      &DriveServiceInterface::GetChangeList,
      base::Unretained(drive_service_),
      start_changestamp,
      base::Bind(&JobScheduler::OnGetChangeListJobDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 new_job->job_info.job_id,
                 callback));
  new_job->abort_callback = CreateErrorRunCallback(callback);
  StartJob(new_job);
}

void JobScheduler::GetRemainingChangeList(
    const GURL& next_link,
    const google_apis::ChangeListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  JobEntry* new_job = CreateNewJob(TYPE_GET_REMAINING_CHANGE_LIST);
  new_job->task = base::Bind(
      &DriveServiceInterface::GetRemainingChangeList,
      base::Unretained(drive_service_),
      next_link,
      base::Bind(&JobScheduler::OnGetChangeListJobDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 new_job->job_info.job_id,
                 callback));
  new_job->abort_callback = CreateErrorRunCallback(callback);
  StartJob(new_job);
}

void JobScheduler::GetRemainingFileList(
    const GURL& next_link,
    const google_apis::FileListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  JobEntry* new_job = CreateNewJob(TYPE_GET_REMAINING_FILE_LIST);
  new_job->task = base::Bind(
      &DriveServiceInterface::GetRemainingFileList,
      base::Unretained(drive_service_),
      next_link,
      base::Bind(&JobScheduler::OnGetFileListJobDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 new_job->job_info.job_id,
                 callback));
  new_job->abort_callback = CreateErrorRunCallback(callback);
  StartJob(new_job);
}

void JobScheduler::GetFileResource(
    const std::string& resource_id,
    const ClientContext& context,
    const google_apis::FileResourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  JobEntry* new_job = CreateNewJob(TYPE_GET_RESOURCE_ENTRY);
  new_job->context = context;
  new_job->task = base::Bind(
      &DriveServiceInterface::GetFileResource,
      base::Unretained(drive_service_),
      resource_id,
      base::Bind(&JobScheduler::OnGetFileResourceJobDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 new_job->job_info.job_id,
                 callback));
  new_job->abort_callback = CreateErrorRunCallback(callback);
  StartJob(new_job);
}

void JobScheduler::GetShareUrl(
    const std::string& resource_id,
    const GURL& embed_origin,
    const ClientContext& context,
    const google_apis::GetShareUrlCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  JobEntry* new_job = CreateNewJob(TYPE_GET_SHARE_URL);
  new_job->context = context;
  new_job->task = base::Bind(
      &DriveServiceInterface::GetShareUrl,
      base::Unretained(drive_service_),
      resource_id,
      embed_origin,
      base::Bind(&JobScheduler::OnGetShareUrlJobDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 new_job->job_info.job_id,
                 callback));
  new_job->abort_callback = CreateErrorRunCallback(callback);
  StartJob(new_job);
}

void JobScheduler::TrashResource(
    const std::string& resource_id,
    const ClientContext& context,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  JobEntry* new_job = CreateNewJob(TYPE_TRASH_RESOURCE);
  new_job->context = context;
  new_job->task = base::Bind(
      &DriveServiceInterface::TrashResource,
      base::Unretained(drive_service_),
      resource_id,
      base::Bind(&JobScheduler::OnEntryActionJobDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 new_job->job_info.job_id,
                 callback));
  new_job->abort_callback = callback;
  StartJob(new_job);
}

void JobScheduler::CopyResource(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& new_title,
    const base::Time& last_modified,
    const google_apis::FileResourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  JobEntry* new_job = CreateNewJob(TYPE_COPY_RESOURCE);
  new_job->task = base::Bind(
      &DriveServiceInterface::CopyResource,
      base::Unretained(drive_service_),
      resource_id,
      parent_resource_id,
      new_title,
      last_modified,
      base::Bind(&JobScheduler::OnGetFileResourceJobDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 new_job->job_info.job_id,
                 callback));
  new_job->abort_callback = CreateErrorRunCallback(callback);
  StartJob(new_job);
}

void JobScheduler::UpdateResource(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& new_title,
    const base::Time& last_modified,
    const base::Time& last_viewed_by_me,
    const ClientContext& context,
    const google_apis::FileResourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  JobEntry* new_job = CreateNewJob(TYPE_UPDATE_RESOURCE);
  new_job->context = context;
  new_job->task = base::Bind(
      &DriveServiceInterface::UpdateResource,
      base::Unretained(drive_service_),
      resource_id,
      parent_resource_id,
      new_title,
      last_modified,
      last_viewed_by_me,
      base::Bind(&JobScheduler::OnGetFileResourceJobDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 new_job->job_info.job_id,
                 callback));
  new_job->abort_callback = CreateErrorRunCallback(callback);
  StartJob(new_job);
}

void JobScheduler::RenameResource(
    const std::string& resource_id,
    const std::string& new_title,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  JobEntry* new_job = CreateNewJob(TYPE_RENAME_RESOURCE);
  new_job->task = base::Bind(
      &DriveServiceInterface::RenameResource,
      base::Unretained(drive_service_),
      resource_id,
      new_title,
      base::Bind(&JobScheduler::OnEntryActionJobDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 new_job->job_info.job_id,
                 callback));
  new_job->abort_callback = callback;
  StartJob(new_job);
}

void JobScheduler::AddResourceToDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  JobEntry* new_job = CreateNewJob(TYPE_ADD_RESOURCE_TO_DIRECTORY);
  new_job->task = base::Bind(
      &DriveServiceInterface::AddResourceToDirectory,
      base::Unretained(drive_service_),
      parent_resource_id,
      resource_id,
      base::Bind(&JobScheduler::OnEntryActionJobDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 new_job->job_info.job_id,
                 callback));
  new_job->abort_callback = callback;
  StartJob(new_job);
}

void JobScheduler::RemoveResourceFromDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const ClientContext& context,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  JobEntry* new_job = CreateNewJob(TYPE_REMOVE_RESOURCE_FROM_DIRECTORY);
  new_job->context = context;
  new_job->task = base::Bind(
      &DriveServiceInterface::RemoveResourceFromDirectory,
      base::Unretained(drive_service_),
      parent_resource_id,
      resource_id,
      base::Bind(&JobScheduler::OnEntryActionJobDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 new_job->job_info.job_id,
                 callback));
  new_job->abort_callback = callback;
  StartJob(new_job);
}

void JobScheduler::AddNewDirectory(
    const std::string& parent_resource_id,
    const std::string& directory_title,
    const DriveServiceInterface::AddNewDirectoryOptions& options,
    const ClientContext& context,
    const google_apis::FileResourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  JobEntry* new_job = CreateNewJob(TYPE_ADD_NEW_DIRECTORY);
  new_job->context = context;
  new_job->task = base::Bind(
      &DriveServiceInterface::AddNewDirectory,
      base::Unretained(drive_service_),
      parent_resource_id,
      directory_title,
      options,
      base::Bind(&JobScheduler::OnGetFileResourceJobDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 new_job->job_info.job_id,
                 callback));
  new_job->abort_callback = CreateErrorRunCallback(callback);
  StartJob(new_job);
}

JobID JobScheduler::DownloadFile(
    const base::FilePath& virtual_path,
    int64 expected_file_size,
    const base::FilePath& local_cache_path,
    const std::string& resource_id,
    const ClientContext& context,
    const google_apis::DownloadActionCallback& download_action_callback,
    const google_apis::GetContentCallback& get_content_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  JobEntry* new_job = CreateNewJob(TYPE_DOWNLOAD_FILE);
  new_job->job_info.file_path = virtual_path;
  new_job->job_info.num_total_bytes = expected_file_size;
  new_job->context = context;
  new_job->task = base::Bind(
      &DriveServiceInterface::DownloadFile,
      base::Unretained(drive_service_),
      local_cache_path,
      resource_id,
      base::Bind(&JobScheduler::OnDownloadActionJobDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 new_job->job_info.job_id,
                 download_action_callback),
      get_content_callback,
      base::Bind(&JobScheduler::UpdateProgress,
                 weak_ptr_factory_.GetWeakPtr(),
                 new_job->job_info.job_id));
  new_job->abort_callback = CreateErrorRunCallback(download_action_callback);
  StartJob(new_job);
  return new_job->job_info.job_id;
}

void JobScheduler::UploadNewFile(
    const std::string& parent_resource_id,
    const base::FilePath& drive_file_path,
    const base::FilePath& local_file_path,
    const std::string& title,
    const std::string& content_type,
    const DriveUploader::UploadNewFileOptions& options,
    const ClientContext& context,
    const google_apis::FileResourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  JobEntry* new_job = CreateNewJob(TYPE_UPLOAD_NEW_FILE);
  new_job->job_info.file_path = drive_file_path;
  new_job->context = context;

  UploadNewFileParams params;
  params.parent_resource_id = parent_resource_id;
  params.local_file_path = local_file_path;
  params.title = title;
  params.content_type = content_type;
  params.options = options;

  ResumeUploadParams resume_params;
  resume_params.local_file_path = params.local_file_path;
  resume_params.content_type = params.content_type;

  params.callback = base::Bind(&JobScheduler::OnUploadCompletionJobDone,
                               weak_ptr_factory_.GetWeakPtr(),
                               new_job->job_info.job_id,
                               resume_params,
                               callback);
  params.progress_callback = base::Bind(&JobScheduler::UpdateProgress,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        new_job->job_info.job_id);
  new_job->task = base::Bind(&RunUploadNewFile, uploader_.get(), params);
  new_job->abort_callback = CreateErrorRunCallback(callback);
  StartJob(new_job);
}

void JobScheduler::UploadExistingFile(
    const std::string& resource_id,
    const base::FilePath& drive_file_path,
    const base::FilePath& local_file_path,
    const std::string& content_type,
    const DriveUploader::UploadExistingFileOptions& options,
    const ClientContext& context,
    const google_apis::FileResourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  JobEntry* new_job = CreateNewJob(TYPE_UPLOAD_EXISTING_FILE);
  new_job->job_info.file_path = drive_file_path;
  new_job->context = context;

  UploadExistingFileParams params;
  params.resource_id = resource_id;
  params.local_file_path = local_file_path;
  params.content_type = content_type;
  params.options = options;

  ResumeUploadParams resume_params;
  resume_params.local_file_path = params.local_file_path;
  resume_params.content_type = params.content_type;

  params.callback = base::Bind(&JobScheduler::OnUploadCompletionJobDone,
                               weak_ptr_factory_.GetWeakPtr(),
                               new_job->job_info.job_id,
                               resume_params,
                               callback);
  params.progress_callback = base::Bind(&JobScheduler::UpdateProgress,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        new_job->job_info.job_id);
  new_job->task = base::Bind(&RunUploadExistingFile, uploader_.get(), params);
  new_job->abort_callback = CreateErrorRunCallback(callback);
  StartJob(new_job);
}

void JobScheduler::AddPermission(
    const std::string& resource_id,
    const std::string& email,
    google_apis::drive::PermissionRole role,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  JobEntry* new_job = CreateNewJob(TYPE_ADD_PERMISSION);
  new_job->task = base::Bind(&DriveServiceInterface::AddPermission,
                             base::Unretained(drive_service_),
                             resource_id,
                             email,
                             role,
                             base::Bind(&JobScheduler::OnEntryActionJobDone,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        new_job->job_info.job_id,
                                        callback));
  new_job->abort_callback = callback;
  StartJob(new_job);
}

JobScheduler::JobEntry* JobScheduler::CreateNewJob(JobType type) {
  JobEntry* job = new JobEntry(type);
  job->job_info.job_id = job_map_.Add(job);  // Takes the ownership of |job|.
  return job;
}

void JobScheduler::StartJob(JobEntry* job) {
  DCHECK(!job->task.is_null());

  QueueJob(job->job_info.job_id);
  NotifyJobAdded(job->job_info);
  DoJobLoop(GetJobQueueType(job->job_info.job_type));
}

void JobScheduler::QueueJob(JobID job_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  JobEntry* job_entry = job_map_.Lookup(job_id);
  DCHECK(job_entry);
  const JobInfo& job_info = job_entry->job_info;

  QueueType queue_type = GetJobQueueType(job_info.job_type);
  queue_[queue_type]->Push(job_id, job_entry->context.type);

  const std::string retry_prefix = job_entry->retry_count > 0 ?
      base::StringPrintf(" (retry %d)", job_entry->retry_count) : "";
  logger_->Log(logging::LOG_INFO,
               "Job queued%s: %s - %s",
               retry_prefix.c_str(),
               job_info.ToString().c_str(),
               GetQueueInfo(queue_type).c_str());
}

void JobScheduler::DoJobLoop(QueueType queue_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const int accepted_priority = GetCurrentAcceptedPriority(queue_type);

  // Abort all USER_INITAITED jobs when not accepted.
  if (accepted_priority < USER_INITIATED) {
    std::vector<JobID> jobs;
    queue_[queue_type]->GetQueuedJobs(USER_INITIATED, &jobs);
    for (size_t i = 0; i < jobs.size(); ++i) {
      JobEntry* job = job_map_.Lookup(jobs[i]);
      DCHECK(job);
      AbortNotRunningJob(job, google_apis::GDATA_NO_CONNECTION);
    }
  }

  // Wait when throttled.
  const base::Time now = base::Time::Now();
  if (now < wait_until_) {
    base::MessageLoopProxy::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&JobScheduler::DoJobLoop,
                   weak_ptr_factory_.GetWeakPtr(),
                   queue_type),
        wait_until_ - now);
    return;
  }

  // Run the job with the highest priority in the queue.
  JobID job_id = -1;
  if (!queue_[queue_type]->PopForRun(accepted_priority, &job_id))
    return;

  JobEntry* entry = job_map_.Lookup(job_id);
  DCHECK(entry);

  JobInfo* job_info = &entry->job_info;
  job_info->state = STATE_RUNNING;
  job_info->start_time = now;
  NotifyJobUpdated(*job_info);

  entry->cancel_callback = entry->task.Run();

  UpdateWait();

  logger_->Log(logging::LOG_INFO,
               "Job started: %s - %s",
               job_info->ToString().c_str(),
               GetQueueInfo(queue_type).c_str());
}

int JobScheduler::GetCurrentAcceptedPriority(QueueType queue_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const int kNoJobShouldRun = -1;

  // Should stop if Drive was disabled while running the fetch loop.
  if (pref_service_->GetBoolean(prefs::kDisableDrive))
    return kNoJobShouldRun;

  // Should stop if the network is not online.
  if (net::NetworkChangeNotifier::IsOffline())
    return kNoJobShouldRun;

  // For the file queue, if it is on cellular network, only user initiated
  // operations are allowed to start.
  if (queue_type == FILE_QUEUE &&
      pref_service_->GetBoolean(prefs::kDisableDriveOverCellular) &&
      net::NetworkChangeNotifier::IsConnectionCellular(
          net::NetworkChangeNotifier::GetConnectionType()))
    return USER_INITIATED;

  // Otherwise, every operations including background tasks are allowed.
  return BACKGROUND;
}

void JobScheduler::UpdateWait() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (disable_throttling_ || throttle_count_ == 0)
    return;

  // Exponential backoff: https://developers.google.com/drive/handle-errors.
  base::TimeDelta delay =
      base::TimeDelta::FromSeconds(1 << (throttle_count_ - 1)) +
      base::TimeDelta::FromMilliseconds(base::RandInt(0, 1000));
  VLOG(1) << "Throttling for " << delay.InMillisecondsF();

  wait_until_ = std::max(wait_until_, base::Time::Now() + delay);
}

bool JobScheduler::OnJobDone(JobID job_id, google_apis::GDataErrorCode error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  JobEntry* job_entry = job_map_.Lookup(job_id);
  DCHECK(job_entry);
  JobInfo* job_info = &job_entry->job_info;
  QueueType queue_type = GetJobQueueType(job_info->job_type);
  queue_[queue_type]->MarkFinished(job_id);

  const base::TimeDelta elapsed = base::Time::Now() - job_info->start_time;
  bool success = (GDataToFileError(error) == FILE_ERROR_OK);
  logger_->Log(success ? logging::LOG_INFO : logging::LOG_WARNING,
               "Job done: %s => %s (elapsed time: %sms) - %s",
               job_info->ToString().c_str(),
               GDataErrorCodeToString(error).c_str(),
               base::Int64ToString(elapsed.InMilliseconds()).c_str(),
               GetQueueInfo(queue_type).c_str());

  // Retry, depending on the error.
  const bool is_server_error =
      error == google_apis::HTTP_SERVICE_UNAVAILABLE ||
      error == google_apis::HTTP_INTERNAL_SERVER_ERROR;
  if (is_server_error) {
    if (throttle_count_ < kMaxThrottleCount)
      ++throttle_count_;
    UpdateWait();
  } else {
    throttle_count_ = 0;
  }

  const bool should_retry =
      is_server_error && job_entry->retry_count < kMaxRetryCount;
  if (should_retry) {
    job_entry->cancel_callback.Reset();
    job_info->state = STATE_RETRY;
    NotifyJobUpdated(*job_info);

    ++job_entry->retry_count;

    // Requeue the job.
    QueueJob(job_id);
  } else {
    NotifyJobDone(*job_info, error);
    // The job has finished, no retry will happen in the scheduler. Now we can
    // remove the job info from the map.
    job_map_.Remove(job_id);
  }

  // Post a task to continue the job loop.  This allows us to finish handling
  // the current job before starting the next one.
  base::MessageLoopProxy::current()->PostTask(FROM_HERE,
      base::Bind(&JobScheduler::DoJobLoop,
                 weak_ptr_factory_.GetWeakPtr(),
                 queue_type));
  return !should_retry;
}

void JobScheduler::OnGetFileListJobDone(
    JobID job_id,
    const google_apis::FileListCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::FileList> file_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (OnJobDone(job_id, error))
    callback.Run(error, file_list.Pass());
}

void JobScheduler::OnGetChangeListJobDone(
    JobID job_id,
    const google_apis::ChangeListCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ChangeList> change_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (OnJobDone(job_id, error))
    callback.Run(error, change_list.Pass());
}

void JobScheduler::OnGetFileResourceJobDone(
    JobID job_id,
    const google_apis::FileResourceCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::FileResource> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (OnJobDone(job_id, error))
    callback.Run(error, entry.Pass());
}

void JobScheduler::OnGetAboutResourceJobDone(
    JobID job_id,
    const google_apis::AboutResourceCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::AboutResource> about_resource) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (OnJobDone(job_id, error))
    callback.Run(error, about_resource.Pass());
}

void JobScheduler::OnGetShareUrlJobDone(
    JobID job_id,
    const google_apis::GetShareUrlCallback& callback,
    google_apis::GDataErrorCode error,
    const GURL& share_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (OnJobDone(job_id, error))
    callback.Run(error, share_url);
}

void JobScheduler::OnGetAppListJobDone(
    JobID job_id,
    const google_apis::AppListCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::AppList> app_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (OnJobDone(job_id, error))
    callback.Run(error, app_list.Pass());
}

void JobScheduler::OnEntryActionJobDone(
    JobID job_id,
    const google_apis::EntryActionCallback& callback,
    google_apis::GDataErrorCode error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (OnJobDone(job_id, error))
    callback.Run(error);
}

void JobScheduler::OnDownloadActionJobDone(
    JobID job_id,
    const google_apis::DownloadActionCallback& callback,
    google_apis::GDataErrorCode error,
    const base::FilePath& temp_file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (OnJobDone(job_id, error))
    callback.Run(error, temp_file);
}

void JobScheduler::OnUploadCompletionJobDone(
    JobID job_id,
    const ResumeUploadParams& resume_params,
    const google_apis::FileResourceCallback& callback,
    google_apis::GDataErrorCode error,
    const GURL& upload_location,
    scoped_ptr<google_apis::FileResource> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (!upload_location.is_empty()) {
    // If upload_location is available, update the task to resume the
    // upload process from the terminated point.
    // When we need to retry, the error code should be HTTP_SERVICE_UNAVAILABLE
    // so OnJobDone called below will be in charge to re-queue the job.
    JobEntry* job_entry = job_map_.Lookup(job_id);
    DCHECK(job_entry);

    ResumeUploadFileParams params;
    params.upload_location = upload_location;
    params.local_file_path = resume_params.local_file_path;
    params.content_type = resume_params.content_type;
    params.callback = base::Bind(&JobScheduler::OnResumeUploadFileDone,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 job_id,
                                 job_entry->task,
                                 callback);
    params.progress_callback = base::Bind(&JobScheduler::UpdateProgress,
                                          weak_ptr_factory_.GetWeakPtr(),
                                          job_id);
    job_entry->task = base::Bind(&RunResumeUploadFile, uploader_.get(), params);
  }

  if (OnJobDone(job_id, error))
    callback.Run(error, entry.Pass());
}

void JobScheduler::OnResumeUploadFileDone(
    JobID job_id,
    const base::Callback<google_apis::CancelCallback()>& original_task,
    const google_apis::FileResourceCallback& callback,
    google_apis::GDataErrorCode error,
    const GURL& upload_location,
    scoped_ptr<google_apis::FileResource> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!original_task.is_null());
  DCHECK(!callback.is_null());

  if (upload_location.is_empty()) {
    // If upload_location is not available, we should discard it and stop trying
    // to resume. Restore the original task.
    JobEntry* job_entry = job_map_.Lookup(job_id);
    DCHECK(job_entry);
    job_entry->task = original_task;
  }

  if (OnJobDone(job_id, error))
    callback.Run(error, entry.Pass());
}

void JobScheduler::UpdateProgress(JobID job_id, int64 progress, int64 total) {
  JobEntry* job_entry = job_map_.Lookup(job_id);
  DCHECK(job_entry);

  job_entry->job_info.num_completed_bytes = progress;
  if (total != -1)
    job_entry->job_info.num_total_bytes = total;
  NotifyJobUpdated(job_entry->job_info);
}

void JobScheduler::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Resume the job loop.
  // Note that we don't need to check the network connection status as it will
  // be checked in GetCurrentAcceptedPriority().
  for (int i = METADATA_QUEUE; i < NUM_QUEUES; ++i)
    DoJobLoop(static_cast<QueueType>(i));
}

JobScheduler::QueueType JobScheduler::GetJobQueueType(JobType type) {
  switch (type) {
    case TYPE_GET_ABOUT_RESOURCE:
    case TYPE_GET_APP_LIST:
    case TYPE_GET_ALL_RESOURCE_LIST:
    case TYPE_GET_RESOURCE_LIST_IN_DIRECTORY:
    case TYPE_SEARCH:
    case TYPE_GET_CHANGE_LIST:
    case TYPE_GET_REMAINING_CHANGE_LIST:
    case TYPE_GET_REMAINING_FILE_LIST:
    case TYPE_GET_RESOURCE_ENTRY:
    case TYPE_GET_SHARE_URL:
    case TYPE_TRASH_RESOURCE:
    case TYPE_COPY_RESOURCE:
    case TYPE_UPDATE_RESOURCE:
    case TYPE_RENAME_RESOURCE:
    case TYPE_ADD_RESOURCE_TO_DIRECTORY:
    case TYPE_REMOVE_RESOURCE_FROM_DIRECTORY:
    case TYPE_ADD_NEW_DIRECTORY:
    case TYPE_CREATE_FILE:
    case TYPE_ADD_PERMISSION:
      return METADATA_QUEUE;

    case TYPE_DOWNLOAD_FILE:
    case TYPE_UPLOAD_NEW_FILE:
    case TYPE_UPLOAD_EXISTING_FILE:
      return FILE_QUEUE;
  }
  NOTREACHED();
  return FILE_QUEUE;
}

void JobScheduler::AbortNotRunningJob(JobEntry* job,
                                      google_apis::GDataErrorCode error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const base::TimeDelta elapsed = base::Time::Now() - job->job_info.start_time;
  const QueueType queue_type = GetJobQueueType(job->job_info.job_type);
  logger_->Log(logging::LOG_INFO,
               "Job aborted: %s => %s (elapsed time: %sms) - %s",
               job->job_info.ToString().c_str(),
               GDataErrorCodeToString(error).c_str(),
               base::Int64ToString(elapsed.InMilliseconds()).c_str(),
               GetQueueInfo(queue_type).c_str());

  base::Callback<void(google_apis::GDataErrorCode)> callback =
      job->abort_callback;
  queue_[GetJobQueueType(job->job_info.job_type)]->Remove(job->job_info.job_id);
  NotifyJobDone(job->job_info, error);
  job_map_.Remove(job->job_info.job_id);
  base::MessageLoopProxy::current()->PostTask(FROM_HERE,
                                              base::Bind(callback, error));
}

void JobScheduler::NotifyJobAdded(const JobInfo& job_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(JobListObserver, observer_list_, OnJobAdded(job_info));
}

void JobScheduler::NotifyJobDone(const JobInfo& job_info,
                                 google_apis::GDataErrorCode error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(JobListObserver, observer_list_,
                    OnJobDone(job_info, GDataToFileError(error)));
}

void JobScheduler::NotifyJobUpdated(const JobInfo& job_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(JobListObserver, observer_list_, OnJobUpdated(job_info));
}

std::string JobScheduler::GetQueueInfo(QueueType type) const {
  return QueueTypeToString(type) + " " + queue_[type]->ToString();
}

// static
std::string JobScheduler::QueueTypeToString(QueueType type) {
  switch (type) {
    case METADATA_QUEUE:
      return "METADATA_QUEUE";
    case FILE_QUEUE:
      return "FILE_QUEUE";
    case NUM_QUEUES:
      break;  // This value is just a sentinel. Should never be used.
  }
  NOTREACHED();
  return "";
}

}  // namespace drive
