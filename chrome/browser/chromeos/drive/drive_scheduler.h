// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_SCHEDULER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_SCHEDULER_H_

#include <list>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/drive/drive_file_system_interface.h"
#include "chrome/browser/google_apis/drive_service_interface.h"
#include "chrome/browser/google_apis/drive_uploader.h"
#include "net/base/network_change_notifier.h"

class Profile;

namespace drive {

// The DriveScheduler is responsible for queuing and scheduling drive
// operations.  It is responsible for handling retry logic, rate limiting, as
// concurrency as appropriate.
//
// TODO(zork): Provide an interface for querying the number of jobs, and state
// of each.  See: crbug.com/154243
class DriveScheduler
    : public net::NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  // Enum representing the type of job.
  enum JobType {
    TYPE_GET_ABOUT_RESOURCE,
    TYPE_GET_ACCOUNT_METADATA,
    TYPE_GET_APP_LIST,
    TYPE_GET_RESOURCE_LIST,
    TYPE_GET_RESOURCE_ENTRY,
    TYPE_DELETE_RESOURCE,
    TYPE_COPY_HOSTED_DOCUMENT,
    TYPE_RENAME_RESOURCE,
    TYPE_ADD_RESOURCE_TO_DIRECTORY,
    TYPE_REMOVE_RESOURCE_FROM_DIRECTORY,
    TYPE_ADD_NEW_DIRECTORY,
    TYPE_DOWNLOAD_FILE,
    TYPE_UPLOAD_EXISTING_FILE,
  };

  // Current state of the job.
  enum JobState {
    // The job is queued, but not yet executed.
    STATE_NONE,

    // The job is in the process of being handled.
    STATE_RUNNING,

    // The job failed, but has been re-added to the queue.
    STATE_RETRY,
  };

  // Information about a specific job that is visible to other systems.
  struct JobInfo {
    explicit JobInfo(JobType in_job_type);

    // Type of the job.
    JobType job_type;

    // Id of the job, which can be used to query or modify it.
    int job_id;

    // Number of bytes completed, if applicable.
    int completed_bytes;

    // Total bytes of this operation, if applicable.
    int total_bytes;

    // Drive path of the file that this job acts on.
    base::FilePath file_path;

    // Current state of the operation.
    JobState state;
  };

  DriveScheduler(Profile* profile,
                 google_apis::DriveServiceInterface* drive_service,
                 google_apis::DriveUploaderInterface* uploader);
  virtual ~DriveScheduler();

  // Initializes the object. This function should be called before any
  // other functions.
  void Initialize();

  // Adds a GetAccountMetadata operation to the queue.
  // |callback| must not be null.
  void GetAccountMetadata(
      const google_apis::GetAccountMetadataCallback& callback);

  // Adds a GetAppList operation to the queue.
  // |callback| must not be null.
  void GetAppList(const google_apis::GetAppListCallback& callback);

  // Adds a GetAboutResource operation to the queue.
  // |callback| must not be null.
  void GetAboutResource(const google_apis::GetAboutResourceCallback& callback);

  // Adds a GetResourceList operation to the queue.
  // |callback| must not be null.
  void GetResourceList(const GURL& feed_url,
                       int64 start_changestamp,
                       const std::string& search_query,
                       bool shared_with_me,
                       const std::string& directory_resource_id,
                       const google_apis::GetResourceListCallback& callback);

  // Adds a GetResourceEntry operation to the queue.
  void GetResourceEntry(const std::string& resource_id,
                        const DriveClientContext& context,
                        const google_apis::GetResourceEntryCallback& callback);


  // Adds a DeleteResource operation to the queue.
  void DeleteResource(const std::string& resource_id,
                      const google_apis::EntryActionCallback& callback);


  // Adds a CopyHostedDocument operation to the queue.
  void CopyHostedDocument(
      const std::string& resource_id,
      const std::string& new_name,
      const google_apis::GetResourceEntryCallback& callback);

  // Adds a RenameResource operation to the queue.
  void RenameResource(const std::string& resource_id,
                      const std::string& new_name,
                      const google_apis::EntryActionCallback& callback);

  // Adds a AddResourceToDirectory operation to the queue.
  void AddResourceToDirectory(const std::string& parent_resource_id,
                              const std::string& resource_id,
                              const google_apis::EntryActionCallback& callback);

  // Adds a RemoveResourceFromDirectory operation to the queue.
  void RemoveResourceFromDirectory(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      const google_apis::EntryActionCallback& callback);

  // Adds a AddNewDirectory operation to the queue.
  void AddNewDirectory(const std::string& parent_resource_id,
                       const std::string& directory_name,
                       const google_apis::GetResourceEntryCallback& callback);

  // Adds a DownloadFile operation to the queue.
  void DownloadFile(
      const base::FilePath& virtual_path,
      const base::FilePath& local_cache_path,
      const GURL& download_url,
      const DriveClientContext& context,
      const google_apis::DownloadActionCallback& download_action_callback,
      const google_apis::GetContentCallback& get_content_callback);

  // Adds an UploadExistingFile operation to the queue.
  void UploadExistingFile(
      const std::string& resource_id,
      const base::FilePath& drive_file_path,
      const base::FilePath& local_file_path,
      const std::string& content_type,
      const std::string& etag,
      const DriveClientContext& context,
      const google_apis::UploadCompletionCallback& upload_completion_callback);

 private:
  friend class DriveSchedulerTest;

  enum QueueType {
    METADATA_QUEUE,
    FILE_QUEUE,
    NUM_QUEUES
  };

  // Represents a single entry in the job queue.
  struct QueueEntry {
    explicit QueueEntry(JobType in_job_type);
    ~QueueEntry();

    static bool Compare(const QueueEntry* left, const QueueEntry* right);

    JobInfo job_info;

    // Context of the job.
    DriveClientContext context;

    // Resource ID to use for the operation.
    // Used by:
    //   TYPE_GET_RESOURCE_ENTRY
    //   TYPE_DELETE_RESOURCE
    //   TYPE_RENAME_RESOURCE
    //   TYPE_ADD_RESOURCE_TO_DIRECTORY
    //   TYPE_UPLOAD_EXISTING_FILE
    std::string resource_id;

    // URL to access the contents of the operation's target.
    // Used by:
    //   TYPE_DOWNLOAD_FILE
    GURL download_url;

    // Online and cache path of the operation's target.
    // Used by:
    //   TYPE_DOWNLOAD_FILE
    base::FilePath virtual_path;
    base::FilePath local_cache_path;

    // Parameters for GetResourceList().
    // Used by:
    //   TYPE_GET_RESOURCE_LIST
    GURL feed_url;
    int64 start_changestamp;
    std::string search_query;
    bool shared_with_me;
    std::string directory_resource_id;

    // Parameter for copy or rename.
    // Used by:
    //   TYPE_COPY_HOSTED_DOCUMENT
    //   TYPE_RENAME_RESOURCE
    std::string new_name;

    // Parameters for AddNewDirectory
    // Used by:
    //   TYPE_ADD_NEW_DIRECTORY
    //   TYPE_ADD_RESOURCE_TO_DIRECTORY
    //   TYPE_REMOVE_RESOURCE_FROM_DIRECTORY
    std::string parent_resource_id;
    std::string directory_name;

    // Callback for operations that take a GetResourceListCallback.
    // Used by:
    //   TYPE_GET_RESOURCE_LIST
    google_apis::GetResourceListCallback get_resource_list_callback;

    // Callback for operations that take a GetResourceEntryCallback.
    // Used by:
    //   TYPE_GET_RESOURCE_ENTRY,
    //   TYPE_COPY_HOSTED_DOCUMENT,
    //   TYPE_ADD_NEW_DIRECTORY,
    google_apis::GetResourceEntryCallback get_resource_entry_callback;

    // Callback for operations that take a GetAccountMetadataCallback.
    // Used by:
    //   TYPE_GET_ACCOUNT_METADATA,
    google_apis::GetAccountMetadataCallback get_account_metadata_callback;

    // Callback for operations that take a GetAboutResourceCallback.
    // Used by:
    //   TYPE_GET_ABOUT_RESOURCE,
    google_apis::GetAboutResourceCallback get_about_resource_callback;

    // Callback for operations that take a GetAppListCallback.
    // Used by:
    //   TYPE_GET_APP_LIST,
    google_apis::GetAppListCallback get_app_list_callback;

    // Callback for operations that take a EntryActionCallback.
    // Used by:
    //   TYPE_DELETE_RESOURCE,
    //   TYPE_RENAME_RESOURCE,
    //   TYPE_ADD_RESOURCE_TO_DIRECTORY,
    //   TYPE_REMOVE_RESOURCE_FROM_DIRECTORY,
    google_apis::EntryActionCallback entry_action_callback;

    // Callback for operations that take a DownloadActionCallback
    // Used by:
    //   TYPE_DOWNLOAD_FILE
    google_apis::DownloadActionCallback download_action_callback;

    // Callback for result of GetContent.
    // Used by:
    //   TYPE_DOWNLOAD_FILE
    google_apis::GetContentCallback get_content_callback;

    // Parameters for UploadExistingFile
    // Used by:
    //   TYPE_UPLOAD_EXISTING_FILE
    base::FilePath drive_file_path;
    base::FilePath local_file_path;
    std::string content_type;
    std::string etag;
    google_apis::UploadCompletionCallback upload_completion_callback;
  };

  // Adds the specified job to the queue.  Takes ownership of |job|
  void QueueJob(scoped_ptr<QueueEntry> job);

  // Starts the job loop, if it is not already running.
  void StartJobLoop(QueueType queue_type);

  // Determines the next job that should run, and starts it.
  void DoJobLoop(QueueType queue_type);

  // Checks if operations should be suspended, such as if the network is
  // disconnected.
  //
  // Returns true when it should stop, and false if it should continue.
  bool ShouldStopJobLoop(QueueType queue_type,
                         const  DriveClientContext& context);

  // Increases the throttle delay if it's below the maximum value, and posts a
  // task to continue the loop after the delay.
  void ThrottleAndContinueJobLoop(QueueType queue_type);

  // Resets the throttle delay to the initial value, and continues the job loop.
  void ResetThrottleAndContinueJobLoop(QueueType queue_type);

  // Retries the job if needed, otherwise cleans up the job, invokes the
  // callback, and continues the job loop.
  scoped_ptr<QueueEntry> OnJobDone(scoped_ptr<QueueEntry> queue_entry,
                                   DriveFileError error);

  // Callback for job finishing with a GetResourceListCallback.
  void OnGetResourceListJobDone(
      scoped_ptr<QueueEntry> queue_entry,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::ResourceList> resource_list);

  // Callback for job finishing with a GetResourceEntryCallback.
  void OnGetResourceEntryJobDone(
      scoped_ptr<QueueEntry> queue_entry,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::ResourceEntry> entry);

  // Callback for job finishing with a GetAboutResourceCallback.
  void OnGetAboutResourceJobDone(
      scoped_ptr<QueueEntry> queue_entry,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::AboutResource> about_resource);

  // Callback for job finishing with a GetAccountMetadataCallback.
  void OnGetAccountMetadataJobDone(
      scoped_ptr<QueueEntry> queue_entry,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::AccountMetadata> account_metadata);

  // Callback for job finishing with a GetAppListCallback.
  void OnGetAppListJobDone(
      scoped_ptr<QueueEntry> queue_entry,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::AppList> app_list);

  // Callback for job finishing with a EntryActionCallback.
  void OnEntryActionJobDone(scoped_ptr<QueueEntry> queue_entry,
                            google_apis::GDataErrorCode error);

  // Callback for job finishing with a DownloadActionCallback.
  void OnDownloadActionJobDone(scoped_ptr<QueueEntry> queue_entry,
                               google_apis::GDataErrorCode error,
                               const base::FilePath& temp_file);

  // Callback for job finishing with a UploadCompletionCallback.
  void OnUploadCompletionJobDone(
      scoped_ptr<QueueEntry> queue_entry,
      google_apis::DriveUploadError error,
      const base::FilePath& drive_path,
      const base::FilePath& file_path,
      scoped_ptr<google_apis::ResourceEntry> resource_entry);

  // net::NetworkChangeNotifier::ConnectionTypeObserver override.
  virtual void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

  // Get the type of queue the specified job should be put in.
  QueueType GetJobQueueType(JobType type);

  // For testing only.  Disables throttling so that testing is faster.
  void SetDisableThrottling(bool disable) { disable_throttling_ = disable; }

  // True when there is a job running.  Indicates that new jobs should wait to
  // be executed.
  bool job_loop_is_running_[NUM_QUEUES];

  // Next value that should be assigned as a job id.
  int next_job_id_;

  // The number of times operations have failed in a row, capped at
  // kMaxThrottleCount.  This is used to calculate the delay before running the
  // next task.
  int throttle_count_;

  // Disables throttling for testing.
  bool disable_throttling_;

  // The queues of jobs.
  std::list<QueueEntry*> queue_[NUM_QUEUES];

  google_apis::DriveServiceInterface* drive_service_;
  google_apis::DriveUploaderInterface* uploader_;

  Profile* profile_;

  // Whether this instance is initialized or not.
  bool initialized_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DriveScheduler> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DriveScheduler);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_SCHEDULER_H_
