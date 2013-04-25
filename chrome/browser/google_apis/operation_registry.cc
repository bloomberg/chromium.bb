// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/operation_registry.h"

#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace google_apis {

OperationProgressStatus::OperationProgressStatus(const base::FilePath& path)
    : operation_id(-1),
      file_path(path),
      transfer_state(OPERATION_NOT_STARTED) {
}

OperationRegistry::Operation::Operation(OperationRegistry* registry)
    : registry_(registry),
      progress_status_(base::FilePath()) {
}

OperationRegistry::Operation::Operation(OperationRegistry* registry,
                                        const base::FilePath& path)
    : registry_(registry),
      progress_status_(path) {
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
    registry_->OnOperationStart(this, &progress_status_.operation_id);
  }
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

OperationRegistry::OperationRegistry() {
  in_flight_operations_.set_check_on_null_data(true);
}

OperationRegistry::~OperationRegistry() {
  DCHECK(in_flight_operations_.IsEmpty());
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
}

void OperationRegistry::OnOperationFinish(OperationID id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Operation* operation = in_flight_operations_.Lookup(id);
  DCHECK(operation);

  DVLOG(1) << "GDataOperation[" << id << "] finished.";
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

  // Remove the old one and initiate the new operation.
  const OperationProgressStatus& old_status = suspended->progress_status();
  OperationID old_id = old_status.operation_id;
  in_flight_operations_.Remove(old_id);
  new_status->operation_id = in_flight_operations_.Add(operation);
  DVLOG(1) << "GDataOperation[" << old_id << " -> " <<
           new_status->operation_id << "] resumed.";
}

void OperationRegistry::OnOperationSuspend(OperationID id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Operation* operation = in_flight_operations_.Lookup(id);
  DCHECK(operation);

  DVLOG(1) << "GDataOperation[" << id << "] suspended.";
}

}  // namespace google_apis
