// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_SCHEDULER_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_SCHEDULER_H_

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_interface.h"
#include "net/base/network_change_notifier.h"

#include <deque>

class Profile;

namespace gdata {

namespace file_system {
class RemoveOperation;
}

// The DriveScheduler is responsible for queuing and scheduling drive
// operations.  It is responisble for handling retry logic, rate limiting, as
// concurrency as appropriate.
//
// TODO(zork): Provide an interface for querying the number of jobs, and state
// of each.  See: crbug.com/154243
class DriveScheduler
    : public net::NetworkChangeNotifier::ConnectionTypeObserver {
 public:

  // Enum representing the type of job.
  enum JobType {
    TYPE_REMOVE,
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
                 file_system::RemoveOperation* remove_operation);
  virtual ~DriveScheduler();

  // Initializes the object. This function should be called before any
  // other functions.
  void Initialize();

  // Adds a remove operation to the queue.
  void Remove(const FilePath& file_path,
              bool is_recursive,
              const FileOperationCallback& callback);

 private:
  friend class DriveSchedulerTest;

  // Data specific to a remove operation.
  struct RemoveJobPrivate  {
    RemoveJobPrivate(bool in_is_recursive, FileOperationCallback in_callback);
    ~RemoveJobPrivate();

    bool is_recursive;
    FileOperationCallback callback;
  };

  // Represents a single entry in the job queue.
  struct QueueEntry {
    QueueEntry(JobType in_job_type, FilePath in_file_path);
    ~QueueEntry();

    JobInfo job_info;

    scoped_ptr<RemoveJobPrivate> remove_private;
  };

  // Adds the specified job to the queue.  Takes ownership of |job|
  int QueueJob(QueueEntry* job);

  // Starts the job loop, if it is not already running.
  void StartJobLoop();

  // Detemines the next job that should run, and starts it.
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

  // Callback for job of TYPE_REMOVE finishing.  Retries the job if needed,
  // otherwise cleans up the job, invokes the callback, and continues the job
  // loop.
  void OnRemoveDone(int job_id, DriveFileError error);

  // net::NetworkChangeNotifier::ConnectionTypeObserver override.
  virtual void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

  // For testing only.  Disables throttling so that testing is faster.
  void SetDisableThrottling(bool disable) { disable_throttling_ = disable; }

  // True when there is a job running.  Inidicates that new jobs should wait to
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
  JobMap job_info_;

  // The queue of jobs id.  Sorted by priority.
  std::deque<int> queue_;

  // Drive operations.
  file_system::RemoveOperation* remove_operation_;

  Profile* profile_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DriveScheduler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DriveScheduler);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_SCHEDULER_H_
