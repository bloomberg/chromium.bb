// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/operation_registry.h"

#include "base/strings/string_number_conversions.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

const int64 kNotificationFrequencyInMilliseconds = 1000;

}  // namespace

namespace google_apis {

std::string OperationTypeToString(OperationType type) {
  switch (type) {
    case OPERATION_UPLOAD: return "upload";
    case OPERATION_DOWNLOAD: return "download";
    case OPERATION_OTHER: return "other";
  }
  NOTREACHED();
  return "unknown_transfer_state";
}

std::string OperationTransferStateToString(OperationTransferState state) {
  switch (state) {
    case OPERATION_NOT_STARTED: return "not_started";
    case OPERATION_STARTED: return "started";
    case OPERATION_IN_PROGRESS: return "in_progress";
    case OPERATION_COMPLETED: return "completed";
    case OPERATION_FAILED: return "failed";
    // Suspended state is opaque to users and looks as same as "in_progress".
    case OPERATION_SUSPENDED: return "in_progress";
  }
  NOTREACHED();
  return "unknown_transfer_state";
}

OperationProgressStatus::OperationProgressStatus(OperationType type,
                                                 const base::FilePath& path)
    : operation_id(-1),
      operation_type(type),
      file_path(path),
      transfer_state(OPERATION_NOT_STARTED),
      progress_current(0),
      progress_total(-1) {
}

std::string OperationProgressStatus::DebugString() const {
  std::string str;
  str += "id=";
  str += base::IntToString(operation_id);
  str += " type=";
  str += OperationTypeToString(operation_type);
  str += " path=";
  str += file_path.AsUTF8Unsafe();
  str += " state=";
  str += OperationTransferStateToString(transfer_state);
  str += " progress=";
  str += base::Int64ToString(progress_current);
  str += "/";
  str += base::Int64ToString(progress_total);
  return str;
}

OperationRegistry::Operation::Operation(OperationRegistry* registry)
    : registry_(registry),
      progress_status_(OPERATION_OTHER, base::FilePath()) {
}

OperationRegistry::Operation::Operation(OperationRegistry* registry,
                                        OperationType type,
                                        const base::FilePath& path)
    : registry_(registry),
      progress_status_(type, path) {
}

OperationRegistry::Operation::~Operation() {
  DCHECK(progress_status_.transfer_state == OPERATION_COMPLETED ||
         progress_status_.transfer_state == OPERATION_SUSPENDED ||
         progress_status_.transfer_state == OPERATION_FAILED);
}

void OperationRegistry::Operation::Cancel() {
  DoCancel();
  NotifyFinish(OPERATION_FAILED);
}

void OperationRegistry::Operation::NotifyStart() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Some operations may be restarted. Report only the first "start".
  if (progress_status_.transfer_state == OPERATION_NOT_STARTED) {
    progress_status_.transfer_state = OPERATION_STARTED;
    progress_status_.start_time = base::Time::Now();
    registry_->OnOperationStart(this, &progress_status_.operation_id);
  }
}

void OperationRegistry::Operation::NotifyProgress(
    int64 current, int64 total) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(progress_status_.transfer_state >= OPERATION_STARTED);
  progress_status_.transfer_state = OPERATION_IN_PROGRESS;
  progress_status_.progress_current = current;
  progress_status_.progress_total = total;
  registry_->OnOperationProgress(progress_status().operation_id);
}

void OperationRegistry::Operation::NotifyFinish(
    OperationTransferState status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(progress_status_.transfer_state >= OPERATION_STARTED);
  DCHECK(status == OPERATION_COMPLETED || status == OPERATION_FAILED);
  progress_status_.transfer_state = status;
  registry_->OnOperationFinish(progress_status().operation_id);
}

void OperationRegistry::Operation::NotifySuspend() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(progress_status_.transfer_state >= OPERATION_STARTED);
  progress_status_.transfer_state = OPERATION_SUSPENDED;
  registry_->OnOperationSuspend(progress_status().operation_id);
}

void OperationRegistry::Operation::NotifyResume() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (progress_status_.transfer_state == OPERATION_NOT_STARTED) {
    progress_status_.transfer_state = OPERATION_IN_PROGRESS;
    registry_->OnOperationResume(this, &progress_status_);
  }
}

OperationRegistry::OperationRegistry()
    : do_notification_frequency_control_(true) {
  in_flight_operations_.set_check_on_null_data(true);
}

OperationRegistry::~OperationRegistry() {
  DCHECK(in_flight_operations_.IsEmpty());
}

void OperationRegistry::AddObserver(OperationRegistryObserver* observer) {
  observer_list_.AddObserver(observer);
}

void OperationRegistry::RemoveObserver(OperationRegistryObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void OperationRegistry::DisableNotificationFrequencyControlForTest() {
  do_notification_frequency_control_ = false;
}

void OperationRegistry::CancelAll() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (OperationIDMap::iterator iter(&in_flight_operations_);
       !iter.IsAtEnd();
       iter.Advance()) {
    Operation* operation = iter.GetCurrentValue();
    CancelOperation(operation);
    // CancelOperation may immediately trigger OnOperationFinish and remove the
    // operation from the map, but IDMap is designed to be safe on such remove
    // while iteration.
  }
}

bool OperationRegistry::CancelForFilePath(const base::FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (OperationIDMap::iterator iter(&in_flight_operations_);
       !iter.IsAtEnd();
       iter.Advance()) {
    Operation* operation = iter.GetCurrentValue();
    if (operation->progress_status().file_path == file_path) {
      CancelOperation(operation);
      return true;
    }
  }
  return false;
}

void OperationRegistry::CancelOperation(Operation* operation) {
  if (operation->progress_status().transfer_state == OPERATION_SUSPENDED) {
    // SUSPENDED operation already completed its job (like calling back to
    // its client code). Invoking operation->Cancel() again on it is a kind of
    // 'double deletion'. So here we directly call OnOperationFinish and just
    // unregister the operation from the registry.
    // TODO(kinaba): http://crbug.com/164098 Get rid of the hack.
    OnOperationFinish(operation->progress_status().operation_id);
  } else {
    operation->Cancel();
  }
}

void OperationRegistry::OnOperationStart(
    OperationRegistry::Operation* operation,
    OperationID* id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  *id = in_flight_operations_.Add(operation);
  DVLOG(1) << "GDataOperation[" << *id << "] started.";
  if (IsFileTransferOperation(operation))
    NotifyStatusToObservers();
}

void OperationRegistry::OnOperationProgress(OperationID id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Operation* operation = in_flight_operations_.Lookup(id);
  DCHECK(operation);

  DVLOG(1) << "GDataOperation[" << id << "] "
           << operation->progress_status().DebugString();
  if (IsFileTransferOperation(operation))
    NotifyStatusToObservers();
}

void OperationRegistry::OnOperationFinish(OperationID id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Operation* operation = in_flight_operations_.Lookup(id);
  DCHECK(operation);

  DVLOG(1) << "GDataOperation[" << id << "] finished.";
  if (IsFileTransferOperation(operation))
    NotifyStatusToObservers();
  in_flight_operations_.Remove(id);
}

void OperationRegistry::OnOperationResume(
    OperationRegistry::Operation* operation,
    OperationProgressStatus* new_status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Find the corresponding suspended task.
  Operation* suspended = NULL;
  for (OperationIDMap::iterator iter(&in_flight_operations_);
       !iter.IsAtEnd();
       iter.Advance()) {
    Operation* in_flight_operation = iter.GetCurrentValue();
    const OperationProgressStatus& status =
        in_flight_operation->progress_status();
    if (status.transfer_state == OPERATION_SUSPENDED &&
        status.file_path == operation->progress_status().file_path) {
      suspended = in_flight_operation;
      break;
    }
  }

  if (!suspended) {
    // Preceding suspended operations was not found. Assume it was canceled.
    //
    // operation->Cancel() needs to be called to properly shut down the
    // current operation, but operation->Cancel() tries to unregister itself
    // from the registry. So, as a hack, temporarily assign it an ID.
    // TODO(kinaba): http://crbug.com/164098 Get rid of it.
    new_status->operation_id = in_flight_operations_.Add(operation);
    CancelOperation(operation);
    return;
  }

  // Copy the progress status.
  const OperationProgressStatus& old_status = suspended->progress_status();
  OperationID old_id = old_status.operation_id;

  new_status->progress_current = old_status.progress_current;
  new_status->progress_total = old_status.progress_total;
  new_status->start_time = old_status.start_time;

  // Remove the old one and initiate the new operation.
  in_flight_operations_.Remove(old_id);
  new_status->operation_id = in_flight_operations_.Add(operation);
  DVLOG(1) << "GDataOperation[" << old_id << " -> " <<
           new_status->operation_id << "] resumed.";
  if (IsFileTransferOperation(operation))
    NotifyStatusToObservers();
}

void OperationRegistry::OnOperationSuspend(OperationID id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Operation* operation = in_flight_operations_.Lookup(id);
  DCHECK(operation);

  DVLOG(1) << "GDataOperation[" << id << "] suspended.";
  if (IsFileTransferOperation(operation))
    NotifyStatusToObservers();
}

bool OperationRegistry::IsFileTransferOperation(
    const Operation* operation) const {
  OperationType type = operation->progress_status().operation_type;
  return type == OPERATION_UPLOAD || type == OPERATION_DOWNLOAD;
}

OperationProgressStatusList OperationRegistry::GetProgressStatusList() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  OperationProgressStatusList status_list;
  for (OperationIDMap::const_iterator iter(&in_flight_operations_);
       !iter.IsAtEnd();
       iter.Advance()) {
    const Operation* operation = iter.GetCurrentValue();
    if (IsFileTransferOperation(operation))
      status_list.push_back(operation->progress_status());
  }
  return status_list;
}

bool OperationRegistry::ShouldNotifyStatusNow(
    const OperationProgressStatusList& list) {
  if (!do_notification_frequency_control_)
    return true;

  base::Time now = base::Time::Now();

  // If it is a first event, or some time abnormality is detected, we should
  // not skip this notification.
  if (last_notification_.is_null() || now < last_notification_) {
    last_notification_ = now;
    return true;
  }

  // If sufficiently long time has elapsed since the previous event, we should
  // not skip this notification.
  if ((now - last_notification_).InMilliseconds() >=
      kNotificationFrequencyInMilliseconds) {
    last_notification_ = now;
    return true;
  }

  // If important events (OPERATION_STARTED, COMPLETED, or FAILED) are there,
  // we should not skip this notification.
  for (size_t i = 0; i < list.size(); ++i) {
    if (list[i].transfer_state != OPERATION_IN_PROGRESS) {
      last_notification_ = now;
      return true;
    }
  }

  // Otherwise we can skip it.
  return false;
}

void OperationRegistry::NotifyStatusToObservers() {
  OperationProgressStatusList list(GetProgressStatusList());
  if (ShouldNotifyStatusNow(list))
    FOR_EACH_OBSERVER(OperationRegistryObserver,
                      observer_list_,
                      OnProgressUpdate(list));
}

}  // namespace google_apis
