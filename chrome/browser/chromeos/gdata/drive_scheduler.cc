// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/drive_scheduler.h"

#include <math.h>

#include "base/message_loop.h"
#include "base/rand_util.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_util.h"
#include "chrome/browser/chromeos/gdata/file_system/remove_operation.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace gdata {

namespace {
const int kMaxThrottleCount = 5;
}

DriveScheduler::JobInfo::JobInfo(JobType in_job_type, FilePath in_file_path)
    : job_type(in_job_type),
      job_id(-1),
      completed_bytes(0),
      total_bytes(0),
      file_path(in_file_path),
      state(STATE_NONE) {
}

DriveScheduler::QueueEntry::QueueEntry(JobType in_job_type,
                                       FilePath in_file_path)
    : job_info(TYPE_REMOVE, in_file_path) {
}

DriveScheduler::QueueEntry::~QueueEntry() {
}

DriveScheduler::RemoveJobPrivate::RemoveJobPrivate(
    bool in_is_recursive,
    FileOperationCallback in_callback)
    : is_recursive(in_is_recursive),
      callback(in_callback) {
}

DriveScheduler::RemoveJobPrivate::~RemoveJobPrivate() {
}

DriveScheduler::DriveScheduler(Profile* profile,
                               file_system::RemoveOperation* remove_operation)
    : job_loop_is_running_(false),
      next_job_id_(0),
      throttle_count_(0),
      disable_throttling_(false),
      remove_operation_(remove_operation),
      profile_(profile),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

DriveScheduler::~DriveScheduler() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

void DriveScheduler::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
}


void DriveScheduler::Remove(const FilePath& file_path,
                            bool is_recursive,
                            const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  QueueEntry* new_job = new QueueEntry(TYPE_REMOVE, file_path);
  new_job->remove_private.reset(new RemoveJobPrivate(is_recursive, callback));

  QueueJob(new_job);

  StartJobLoop();
}

int DriveScheduler::QueueJob(QueueEntry* job) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  int job_id = next_job_id_;
  job->job_info.job_id = job_id;
  next_job_id_++;

  queue_.push_back(job_id);

  DCHECK(job_info_.find(job_id) == job_info_.end());
  job_info_[job_id] = make_linked_ptr(job);

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

  JobMap::iterator job_iter = job_info_.find(job_id);
  DCHECK(job_iter != job_info_.end());

  JobInfo& job_info = job_iter->second->job_info;
  job_info.state = STATE_RUNNING;

  switch (job_info.job_type) {
    case TYPE_REMOVE: {
      DCHECK(job_iter->second->remove_private.get());

      remove_operation_->Remove(job_info.file_path,
                                job_iter->second->remove_private->is_recursive,
                                base::Bind(&DriveScheduler::OnRemoveDone,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           job_id));
    }
    break;
  }

}

bool DriveScheduler::ShouldStopJobLoop() {
  // Should stop if the gdata feature was disabled while running the fetch
  // loop.
  if (profile_->GetPrefs()->GetBoolean(prefs::kDisableGData))
    return true;

  // Should stop if the network is not online.
  if (net::NetworkChangeNotifier::IsOffline())
    return true;

  // Should stop if the current connection is on cellular network, and
  // fetching is disabled over cellular.
  if (profile_->GetPrefs()->GetBoolean(prefs::kDisableGDataOverCellular) &&
      util::IsConnectionTypeCellular())
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

  throttle_count_ = 0;
  DoJobLoop();
}

void DriveScheduler::OnRemoveDone(int job_id, DriveFileError error) {
  JobMap::iterator job_iter = job_info_.find(job_id);
  DCHECK(job_iter != job_info_.end());

  // Retry, depending on the error.
  if (error == DRIVE_FILE_ERROR_FAILED ||
      error == DRIVE_FILE_ERROR_NO_CONNECTION) {
    job_iter->second->job_info.state = STATE_RETRY;

    // Requeue the job.
    queue_.push_back(job_id);
    ThrottleAndContinueJobLoop();
  } else {
    DCHECK(job_iter->second->remove_private.get());

    // Handle the callback.
    if (!job_iter->second->remove_private->callback.is_null()) {
      MessageLoop::current()->PostTask(FROM_HERE,
          base::Bind(job_iter->second->remove_private->callback, error));
    }

    // Delete the job.
    job_info_.erase(job_id);
    ResetThrottleAndContinueJobLoop();
  }
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

}  // namespace gdata
