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

namespace file_system {
class DriveOperations;
}

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
    TYPE_GET_APPLICATION_INFO,
    TYPE_COPY,
    TYPE_GET_DOCUMENTS,
    TYPE_MOVE,
    TYPE_REMOVE,
    TYPE_TRANSFER_LOCAL_TO_REMOTE,
    TYPE_TRANSFER_REGULAR_FILE,
    TYPE_TRANSFER_REMOTE_TO_LOCAL,
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
    JobInfo(JobType in_job_type, FilePath in_file_path);

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
                 google_apis::DriveServiceInterface* drive_service,
                 file_system::DriveOperations* drive_operations);
  virtual ~DriveScheduler();

  // Initializes the object. This function should be called before any
  // other functions.
  void Initialize();

  // Adds a GetAccountMetadata operation to the queue.
  // |callback| must not be null.
  void GetAccountMetadata(const google_apis::GetDataCallback& callback);

  // Adds a GetApplicationInfo operation to the queue.
  // |callback| must not be null.
  void GetApplicationInfo(const google_apis::GetDataCallback& callback);

  // Adds a copy operation to the queue.
  // |callback| must not be null.
  void Copy(const FilePath& src_file_path,
            const FilePath& dest_file_path,
            const FileOperationCallback& callback);

  // Adds a GetDocuments operation to the queue.
  // |callback| must not be null.
  void GetDocuments(const GURL& feed_url,
                    int64 start_changestamp,
                    const std::string& search_query,
                    bool shared_with_me,
                    const std::string& directory_resource_id,
                    const google_apis::GetDataCallback& callback);

  // Adds a transfer operation to the queue.
  // |callback| must not be null.
  void TransferFileFromRemoteToLocal(const FilePath& remote_src_file_path,
                                     const FilePath& local_dest_file_path,
                                     const FileOperationCallback& callback);

  // Adds a transfer operation to the queue.
  // |callback| must not be null.
  void TransferFileFromLocalToRemote(const FilePath& local_src_file_path,
                                     const FilePath& remote_dest_file_path,
                                     const FileOperationCallback& callback);

  // Adds a transfer operation to the queue.
  // |callback| must not be null.
  void TransferRegularFile(const FilePath& local_src_file_path,
                           const FilePath& remote_dest_file_path,
                           const FileOperationCallback& callback);

  // Adds a move operation to the queue.
  // |callback| must not be null.
  void Move(const FilePath& src_file_path,
            const FilePath& dest_file_path,
            const FileOperationCallback& callback);

  // Adds a remove operation to the queue.
  // |callback| must not be null.
  void Remove(const FilePath& file_path,
              bool is_recursive,
              const FileOperationCallback& callback);

 private:
  friend class DriveSchedulerTest;

  // Represents a single entry in the job queue.
  struct QueueEntry {
    QueueEntry(JobType in_job_type,
               FilePath in_file_path);
    ~QueueEntry();

    JobInfo job_info;

    // Callback for operations that take a FileOperationCallback.
    // Used by:
    //   TYPE_COPY,
    //   TYPE_MOVE,
    //   TYPE_REMOVE,
    //   TYPE_TRANSFER_LOCAL_TO_REMOTE,
    //   TYPE_TRANSFER_REGULAR_FILE,
    //   TYPE_TRANSFER_REMOTE_TO_LOCAL
    FileOperationCallback file_operation_callback;

    // Destination of the operation.
    // Used by:
    //   TYPE_COPY,
    //   TYPE_MOVE,
    //   TYPE_TRANSFER_LOCAL_TO_REMOTE,
    //   TYPE_TRANSFER_REGULAR_FILE,
    //   TYPE_TRANSFER_REMOTE_TO_LOCAL
    FilePath dest_file_path;

    // Whether the operation is recursive.
    // Used by:
    //   TYPE_REMOVE
    bool is_recursive;

    // Parameters for GetDocuments().
    // Used by:
    //   TYPE_GET_DOCUMENTS
    GURL feed_url;
    int64 start_changestamp;
    std::string search_query;
    bool shared_with_me;
    std::string directory_resource_id;

    // Callback for operations that take a GetDataCallback.
    // Used by:
    //   TYPE_GET_ACCOUNT_METADATA,
    //   TYPE_GET_APPLICATION_INFO,
    //   TYPE_GET_DOCUMENTS
    google_apis::GetDataCallback get_data_callback;
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

  // Callback for job finishing with a FileOperationCallback.
  void OnFileOperationJobDone(int job_id, DriveFileError error);

  // Callback for job finishing with a GetDataCallback.
  void OnGetDataJobDone(int job_id,
                        google_apis::GDataErrorCode error,
                        scoped_ptr<base::Value> feed_data);

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

  // Drive operations.
  file_system::DriveOperations* drive_operations_;

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
