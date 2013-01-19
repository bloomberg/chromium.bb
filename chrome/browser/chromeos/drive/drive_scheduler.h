// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_SCHEDULER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_SCHEDULER_H_

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/drive/drive_file_system_interface.h"
#include "chrome/browser/google_apis/drive_service_interface.h"
#include "net/base/network_change_notifier.h"

#include <deque>

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
    FilePath file_path;

    // Current state of the operation.
    JobState state;
  };

  DriveScheduler(Profile* profile,
                 google_apis::DriveServiceInterface* drive_service);
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
                        const google_apis::GetResourceEntryCallback& callback);


  // Adds a DeleteResource operation to the queue.
  void DeleteResource(const GURL& edit_url,
                      const google_apis::EntryActionCallback& callback);


  // Adds a CopyHostedDocument operation to the queue.
  void CopyHostedDocument(
      const std::string& resource_id,
      const FilePath::StringType& new_name,
      const google_apis::GetResourceEntryCallback& callback);

  // Adds a RenameResource operation to the queue.
  void RenameResource(const GURL& edit_url,
                      const FilePath::StringType& new_name,
                      const google_apis::EntryActionCallback& callback);

  // Adds a AddResourceToDirectory operation to the queue.
  void AddResourceToDirectory(const GURL& parent_content_url,
                              const GURL& edit_url,
                              const google_apis::EntryActionCallback& callback);

  // Adds a RemoveResourceFromDirectory operation to the queue.
  void RemoveResourceFromDirectory(
      const GURL& parent_content_url,
      const std::string& resource_id,
      const google_apis::EntryActionCallback& callback);

  // Adds a AddNewDirectory operation to the queue.
  void AddNewDirectory(const GURL& parent_content_url,
                       const FilePath::StringType& directory_name,
                       const google_apis::GetResourceEntryCallback& callback);

  // Adds a DownloadFile operation to the queue.
  void DownloadFile(
      const FilePath& virtual_path,
      const FilePath& local_cache_path,
      const GURL& content_url,
      const google_apis::DownloadActionCallback& download_action_callback,
      const google_apis::GetContentCallback& get_content_callback);

 private:
  friend class DriveSchedulerTest;

  // Represents a single entry in the job queue.
  struct QueueEntry {
    explicit QueueEntry(JobType in_job_type);
    ~QueueEntry();

    JobInfo job_info;

    // Resource ID to use for the operation.
    // Used by:
    //   TYPE_GET_RESOURCE_ENTRY
    std::string resource_id;

    // URL to use to modify the resource.
    // Used by:
    //  TYPE_DELETE_RESOURCE
    //  TYPE_RENAME_RESOURCE
    //  TYPE_ADD_NEW_DIRECTORY
    GURL edit_url;

    // URL to access the contents of the operation's target.
    // Used by:
    //   TYPE_DOWNLOAD_FILE
    GURL content_url;

    // Online and cache path of the operation's target.
    // Used by:
    //   TYPE_DOWNLOAD_FILE
    FilePath virtual_path;
    FilePath local_cache_path;

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
    FilePath::StringType new_name;

    // Parameters for AddNewDirectory
    // Used by:
    //   TYPE_ADD_NEW_DIRECTORY
    //   TYPE_ADD_RESOURCE_TO_DIRECTORY
    //   TYPE_REMOVE_RESOURCE_FROM_DIRECTORY
    GURL parent_content_url;
    FilePath::StringType directory_name;

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
  };

  // Adds the specified job to the queue.  Takes ownership of |job|
  int QueueJob(scoped_ptr<QueueEntry> job);

  // Starts the job loop, if it is not already running.
  void StartJobLoop();

  // Determines the next job that should run, and starts it.
  void DoJobLoop();

  // Checks if operations should be suspended, such as if the network is
  // disconnected.
  //
  // Returns true when it should stop, and false if it should continue.
  bool ShouldStopJobLoop();

  // Increases the throttle delay if it's below the maximum value, and posts a
  // task to continue the loop after the delay.
  void ThrottleAndContinueJobLoop();

  // Resets the throttle delay to the initial value, and continues the job loop.
  void ResetThrottleAndContinueJobLoop();

  // Retries the job if needed, otherwise cleans up the job, invokes the
  // callback, and continues the job loop.
  scoped_ptr<QueueEntry> OnJobDone(int job_id, DriveFileError error);

  // Callback for job finishing with a GetResourceListCallback.
  void OnGetResourceListJobDone(
      int job_id,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::ResourceList> resource_list);

  // Callback for job finishing with a GetResourceEntryCallback.
  void OnGetResourceEntryJobDone(
      int job_id,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::ResourceEntry> entry);

  // Callback for job finishing with a GetAccountMetadataCallback.
  void OnGetAccountMetadataJobDone(
      int job_id,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::AccountMetadataFeed> account_metadata);

  // Callback for job finishing with a GetAppListCallback.
  void OnGetAppListJobDone(
      int job_id,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::AppList> app_list);

  // Callback for job finishing with a EntryActionCallback.
  void OnEntryActionJobDone(int job_id, google_apis::GDataErrorCode error);

  // Callback for job finishing with a DownloadActionCallback.
  void OnDownloadActionJobDone(int job_id,
                               google_apis::GDataErrorCode error,
                               const FilePath& temp_file);

  // net::NetworkChangeNotifier::ConnectionTypeObserver override.
  virtual void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

  // For testing only.  Disables throttling so that testing is faster.
  void SetDisableThrottling(bool disable) { disable_throttling_ = disable; }

  // True when there is a job running.  Indicates that new jobs should wait to
  // be executed.
  bool job_loop_is_running_;

  // Next value that should be assigned as a job id.
  int next_job_id_;

  // The number of times operations have failed in a row, capped at
  // kMaxThrottleCount.  This is used to calculate the delay before running the
  // next task.
  int throttle_count_;

  // Disables throttling for testing.
  bool disable_throttling_;

  // Mapping of id to QueueEntry.
  typedef std::map<int, linked_ptr<QueueEntry> > JobMap;
  JobMap job_info_map_;

  // The queue of jobs id.  Sorted by priority.
  std::deque<int> queue_;

  google_apis::DriveServiceInterface* drive_service_;

  Profile* profile_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DriveScheduler> weak_ptr_factory_;

  // Whether this instance is initialized or not.
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(DriveScheduler);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_SCHEDULER_H_
