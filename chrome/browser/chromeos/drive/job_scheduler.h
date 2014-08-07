// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_JOB_SCHEDULER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_JOB_SCHEDULER_H_

#include <vector>

#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/drive/job_list.h"
#include "chrome/browser/chromeos/drive/job_queue.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "chrome/browser/drive/drive_uploader.h"
#include "net/base/network_change_notifier.h"

class PrefService;

namespace base {
class SeqencedTaskRunner;
}

namespace drive {

class EventLogger;

// Priority of a job.  Higher values are lower priority.
enum ContextType {
  USER_INITIATED,
  BACKGROUND,
  // Indicates the number of values of this enum.
  NUM_CONTEXT_TYPES,
};

struct ClientContext {
  explicit ClientContext(ContextType in_type) : type(in_type) {}
  ContextType type;
};

// The JobScheduler is responsible for queuing and scheduling drive jobs.
// Because jobs are executed concurrently by priority and retried for network
// failures, there is no guarantee of orderings.
//
// Jobs are grouped into two priority levels:
//   - USER_INITIATED jobs are those occur as a result of direct user actions.
//   - BACKGROUND jobs runs in response to state changes, server actions, etc.
// USER_INITIATED jobs must be handled immediately, thus have higher priority.
// BACKGROUND jobs run only after all USER_INITIATED jobs have run.
//
// Orthogonally, jobs are grouped into two types:
//   - "File jobs" transfer the contents of files.
//   - "Metadata jobs" operates on file metadata or the directory structure.
// On WiFi or Ethernet connections, all types of jobs just run.
// On mobile connections (2G/3G/4G), we don't want large background traffic.
// USER_INITIATED jobs or metadata jobs will run. BACKGROUND file jobs wait
// in the queue until the network type changes.
// On offline case, no jobs run. USER_INITIATED jobs fail immediately.
// BACKGROUND jobs stay in the queue and wait for network connection.
class JobScheduler
    : public net::NetworkChangeNotifier::ConnectionTypeObserver,
      public JobListInterface {
 public:
  JobScheduler(PrefService* pref_service,
               EventLogger* logger,
               DriveServiceInterface* drive_service,
               base::SequencedTaskRunner* blocking_task_runner);
  virtual ~JobScheduler();

  // JobListInterface overrides.
  virtual std::vector<JobInfo> GetJobInfoList() OVERRIDE;
  virtual void AddObserver(JobListObserver* observer) OVERRIDE;
  virtual void RemoveObserver(JobListObserver* observer) OVERRIDE;
  virtual void CancelJob(JobID job_id) OVERRIDE;
  virtual void CancelAllJobs() OVERRIDE;

  // Adds a GetAppList operation to the queue.
  // |callback| must not be null.
  void GetAppList(const google_apis::AppListCallback& callback);

  // Adds a GetAboutResource operation to the queue.
  // |callback| must not be null.
  void GetAboutResource(const google_apis::AboutResourceCallback& callback);

  // Adds a GetAllFileList operation to the queue.
  // |callback| must not be null.
  void GetAllFileList(const google_apis::FileListCallback& callback);

  // Adds a GetFileListInDirectory operation to the queue.
  // |callback| must not be null.
  void GetFileListInDirectory(const std::string& directory_resource_id,
                              const google_apis::FileListCallback& callback);

  // Adds a Search operation to the queue.
  // |callback| must not be null.
  void Search(const std::string& search_query,
              const google_apis::FileListCallback& callback);

  // Adds a GetChangeList operation to the queue.
  // |callback| must not be null.
  void GetChangeList(int64 start_changestamp,
                     const google_apis::ChangeListCallback& callback);

  // Adds GetRemainingChangeList operation to the queue.
  // |callback| must not be null.
  void GetRemainingChangeList(const GURL& next_link,
                              const google_apis::ChangeListCallback& callback);

  // Adds GetRemainingFileList operation to the queue.
  // |callback| must not be null.
  void GetRemainingFileList(const GURL& next_link,
                            const google_apis::FileListCallback& callback);

  // Adds a GetFileResource operation to the queue.
  void GetFileResource(const std::string& resource_id,
                       const ClientContext& context,
                       const google_apis::FileResourceCallback& callback);

  // Adds a GetShareUrl operation to the queue.
  void GetShareUrl(const std::string& resource_id,
                   const GURL& embed_origin,
                   const ClientContext& context,
                   const google_apis::GetShareUrlCallback& callback);

  // Adds a TrashResource operation to the queue.
  void TrashResource(const std::string& resource_id,
                     const ClientContext& context,
                     const google_apis::EntryActionCallback& callback);

  // Adds a CopyResource operation to the queue.
  void CopyResource(const std::string& resource_id,
                    const std::string& parent_resource_id,
                    const std::string& new_title,
                    const base::Time& last_modified,
                    const google_apis::FileResourceCallback& callback);

  // Adds a UpdateResource operation to the queue.
  void UpdateResource(const std::string& resource_id,
                      const std::string& parent_resource_id,
                      const std::string& new_title,
                      const base::Time& last_modified,
                      const base::Time& last_viewed_by_me,
                      const ClientContext& context,
                      const google_apis::FileResourceCallback& callback);

  // Adds a AddResourceToDirectory operation to the queue.
  void AddResourceToDirectory(const std::string& parent_resource_id,
                              const std::string& resource_id,
                              const google_apis::EntryActionCallback& callback);

  // Adds a RemoveResourceFromDirectory operation to the queue.
  void RemoveResourceFromDirectory(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      const ClientContext& context,
      const google_apis::EntryActionCallback& callback);

  // Adds a AddNewDirectory operation to the queue.
  void AddNewDirectory(
      const std::string& parent_resource_id,
      const std::string& directory_title,
      const DriveServiceInterface::AddNewDirectoryOptions& options,
      const ClientContext& context,
      const google_apis::FileResourceCallback& callback);

  // Adds a DownloadFile operation to the queue.
  // The first two arguments |virtual_path| and |expected_file_size| are used
  // only for filling JobInfo for the operation so that observers can get the
  // detail. The actual operation never refers these values.
  JobID DownloadFile(
      const base::FilePath& virtual_path,
      int64 expected_file_size,
      const base::FilePath& local_cache_path,
      const std::string& resource_id,
      const ClientContext& context,
      const google_apis::DownloadActionCallback& download_action_callback,
      const google_apis::GetContentCallback& get_content_callback);

  // Adds an UploadNewFile operation to the queue.
  void UploadNewFile(const std::string& parent_resource_id,
                     const base::FilePath& drive_file_path,
                     const base::FilePath& local_file_path,
                     const std::string& title,
                     const std::string& content_type,
                     const DriveUploader::UploadNewFileOptions& options,
                     const ClientContext& context,
                     const google_apis::FileResourceCallback& callback);

  // Adds an UploadExistingFile operation to the queue.
  void UploadExistingFile(
      const std::string& resource_id,
      const base::FilePath& drive_file_path,
      const base::FilePath& local_file_path,
      const std::string& content_type,
      const DriveUploader::UploadExistingFileOptions& options,
      const ClientContext& context,
      const google_apis::FileResourceCallback& callback);

  // Adds AddPermission operation to the queue. |callback| must not be null.
  void AddPermission(const std::string& resource_id,
                     const std::string& email,
                     google_apis::drive::PermissionRole role,
                     const google_apis::EntryActionCallback& callback);

 private:
  friend class JobSchedulerTest;

  enum QueueType {
    METADATA_QUEUE,
    FILE_QUEUE,
    NUM_QUEUES
  };

  static const int kMaxJobCount[NUM_QUEUES];

  // Represents a single entry in the job map.
  struct JobEntry {
    explicit JobEntry(JobType type);
    ~JobEntry();

    // General user-visible information on the job.
    JobInfo job_info;

    // Context of the job.
    ClientContext context;

    // The number of times the jobs is retried due to server errors.
    int retry_count;

    // The callback to start the job. Called each time it is retry.
    base::Callback<google_apis::CancelCallback()> task;

    // The callback to cancel the running job. It is returned from task.Run().
    google_apis::CancelCallback cancel_callback;

    // The callback to notify an error to the client of JobScheduler.
    // This is used to notify cancel of a job that is not running yet.
    base::Callback<void(google_apis::GDataErrorCode)> abort_callback;
  };

  // Parameters for DriveUploader::ResumeUploadFile.
  struct ResumeUploadParams;

  // Creates a new job and add it to the job map.
  JobEntry* CreateNewJob(JobType type);

  // Adds the specified job to the queue and starts the job loop for the queue
  // if needed.
  void StartJob(JobEntry* job);

  // Adds the specified job to the queue.
  void QueueJob(JobID job_id);

  // Determines the next job that should run, and starts it.
  void DoJobLoop(QueueType queue_type);

  // Returns the lowest acceptable priority level of the operations that is
  // currently allowed to start for the |queue_type|.
  int GetCurrentAcceptedPriority(QueueType queue_type);

  // Updates |wait_until_| to throttle requests.
  void UpdateWait();

  // Retries the job if needed and returns false. Otherwise returns true.
  bool OnJobDone(JobID job_id, google_apis::GDataErrorCode error);

  // Callback for job finishing with a FileListCallback.
  void OnGetFileListJobDone(
      JobID job_id,
      const google_apis::FileListCallback& callback,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::FileList> file_list);

  // Callback for job finishing with a ChangeListCallback.
  void OnGetChangeListJobDone(
      JobID job_id,
      const google_apis::ChangeListCallback& callback,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::ChangeList> change_list);

  // Callback for job finishing with a FileResourceCallback.
  void OnGetFileResourceJobDone(
      JobID job_id,
      const google_apis::FileResourceCallback& callback,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::FileResource> entry);

  // Callback for job finishing with a AboutResourceCallback.
  void OnGetAboutResourceJobDone(
      JobID job_id,
      const google_apis::AboutResourceCallback& callback,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::AboutResource> about_resource);

  // Callback for job finishing with a GetShareUrlCallback.
  void OnGetShareUrlJobDone(
      JobID job_id,
      const google_apis::GetShareUrlCallback& callback,
      google_apis::GDataErrorCode error,
      const GURL& share_url);

  // Callback for job finishing with a AppListCallback.
  void OnGetAppListJobDone(
      JobID job_id,
      const google_apis::AppListCallback& callback,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::AppList> app_list);

  // Callback for job finishing with a EntryActionCallback.
  void OnEntryActionJobDone(JobID job_id,
                            const google_apis::EntryActionCallback& callback,
                            google_apis::GDataErrorCode error);

  // Callback for job finishing with a DownloadActionCallback.
  void OnDownloadActionJobDone(
      JobID job_id,
      const google_apis::DownloadActionCallback& callback,
      google_apis::GDataErrorCode error,
      const base::FilePath& temp_file);

  // Callback for job finishing with a UploadCompletionCallback.
  void OnUploadCompletionJobDone(
      JobID job_id,
      const ResumeUploadParams& resume_params,
      const google_apis::FileResourceCallback& callback,
      google_apis::GDataErrorCode error,
      const GURL& upload_location,
      scoped_ptr<google_apis::FileResource> entry);

  // Callback for DriveUploader::ResumeUploadFile().
  void OnResumeUploadFileDone(
      JobID job_id,
      const base::Callback<google_apis::CancelCallback()>& original_task,
      const google_apis::FileResourceCallback& callback,
      google_apis::GDataErrorCode error,
      const GURL& upload_location,
      scoped_ptr<google_apis::FileResource> entry);

  // Updates the progress status of the specified job.
  void UpdateProgress(JobID job_id, int64 progress, int64 total);

  // net::NetworkChangeNotifier::ConnectionTypeObserver override.
  virtual void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

  // Get the type of queue the specified job should be put in.
  QueueType GetJobQueueType(JobType type);

  // For testing only.  Disables throttling so that testing is faster.
  void SetDisableThrottling(bool disable) { disable_throttling_ = disable; }

  // Aborts a job which is not in STATE_RUNNING.
  void AbortNotRunningJob(JobEntry* job, google_apis::GDataErrorCode error);

  // Notifies updates to observers.
  void NotifyJobAdded(const JobInfo& job_info);
  void NotifyJobDone(const JobInfo& job_info,
                     google_apis::GDataErrorCode error);
  void NotifyJobUpdated(const JobInfo& job_info);

  // Gets information of the queue of the given type as string.
  std::string GetQueueInfo(QueueType type) const;

  // Returns a string representation of QueueType.
  static std::string QueueTypeToString(QueueType type);

  // The number of times operations have failed in a row, capped at
  // kMaxThrottleCount.  This is used to calculate the delay before running the
  // next task.
  int throttle_count_;

  // Jobs should not start running until this time. Used for throttling.
  base::Time wait_until_;

  // Disables throttling for testing.
  bool disable_throttling_;

  // The queues of jobs.
  scoped_ptr<JobQueue> queue_[NUM_QUEUES];

  // The list of queued job info indexed by job IDs.
  typedef IDMap<JobEntry, IDMapOwnPointer> JobIDMap;
  JobIDMap job_map_;

  // The list of observers for the scheduler.
  ObserverList<JobListObserver> observer_list_;

  EventLogger* logger_;
  DriveServiceInterface* drive_service_;
  scoped_ptr<DriveUploaderInterface> uploader_;

  PrefService* pref_service_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<JobScheduler> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(JobScheduler);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_JOB_SCHEDULER_H_
