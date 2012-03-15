// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_operation_registry.h"

#include <sstream>

#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace gdata {

// static
std::string GDataOperationRegistry::OperationTypeToString(OperationType type) {
  switch (type) {
    case OPERATION_UPLOAD: return "upload";
    case OPERATION_DOWNLOAD: return "download";
    case OPERATION_OTHER: return "other";
  }
  NOTREACHED();
  return "unknown_transfer_state";
}

// static
std::string GDataOperationRegistry::OperationTransferStateToString(
    OperationTransferState state) {
  switch (state) {
    case OPERATION_NOT_STARTED: return "not_started";
    case OPERATION_STARTED: return "started";
    case OPERATION_IN_PROGRESS: return "in_progress";
    case OPERATION_COMPLETED: return "completed";
    case OPERATION_FAILED: return "failed";
  }
  NOTREACHED();
  return "unknown_transfer_state";
}

GDataOperationRegistry::ProgressStatus::ProgressStatus(OperationType type,
                                                       const FilePath& path)
    : operation_id(-1),
      operation_type(type),
      file_path(path),
      transfer_state(OPERATION_NOT_STARTED),
      progress_current(0),
      progress_total(-1) {
}

std::string GDataOperationRegistry::ProgressStatus::ToString() const {
  std::stringstream stream;
  stream << "id=" << operation_id
         << "type="
         << GDataOperationRegistry::OperationTypeToString(operation_type)
         << "path=" << file_path.AsUTF8Unsafe()
         << "state=" << OperationTransferStateToString(transfer_state)
         << ", progress=" << progress_current
         << "/ " << progress_total;
  return stream.str();
}

GDataOperationRegistry::Operation::Operation(GDataOperationRegistry* registry)
    : registry_(registry),
      progress_status_(GDataOperationRegistry::OPERATION_OTHER, FilePath()) {
}

GDataOperationRegistry::Operation::Operation(GDataOperationRegistry* registry,
                                             OperationType type,
                                             const FilePath& path)
    : registry_(registry),
      progress_status_(type, path) {
}

GDataOperationRegistry::Operation::~Operation() {
  DCHECK(progress_status_.transfer_state == OPERATION_COMPLETED
      || progress_status_.transfer_state == OPERATION_FAILED);
}

void GDataOperationRegistry::Operation::Cancel() {
  DoCancel();
  NotifyFinish(OPERATION_FAILED);
}

void GDataOperationRegistry::Operation::NotifyStart() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  progress_status_.transfer_state = OPERATION_STARTED;
  progress_status_.start_time = base::Time::Now();
  registry_->OnOperationStart(this, &progress_status_.operation_id);
}

void GDataOperationRegistry::Operation::NotifyProgress(
    int64 current, int64 total) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(progress_status_.transfer_state >= OPERATION_STARTED);
  progress_status_.transfer_state = OPERATION_IN_PROGRESS;
  progress_status_.progress_current = current;
  progress_status_.progress_total = total;
  registry_->OnOperationProgress(progress_status().operation_id);
}

void GDataOperationRegistry::Operation::NotifyFinish(
    OperationTransferState status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(progress_status_.transfer_state >= OPERATION_STARTED);
  DCHECK(status == OPERATION_COMPLETED || status == OPERATION_FAILED);
  progress_status_.transfer_state = status;
  registry_->OnOperationFinish(progress_status().operation_id);
}

GDataOperationRegistry::GDataOperationRegistry() {
  in_flight_operations_.set_check_on_null_data(true);
}

GDataOperationRegistry::~GDataOperationRegistry() {
  DCHECK(in_flight_operations_.IsEmpty());
}

void GDataOperationRegistry::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void GDataOperationRegistry::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void GDataOperationRegistry::CancelAll() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (OperationIDMap::iterator iter(&in_flight_operations_);
       !iter.IsAtEnd();
       iter.Advance()) {
    Operation* operation = iter.GetCurrentValue();
    operation->Cancel();
    // Cancel() may immediately trigger OnOperationFinish and remove the
    // operation from the map, but IDMap is designed to be safe on such remove
    // while iteration.
  }
}

void GDataOperationRegistry::OnOperationStart(
    GDataOperationRegistry::Operation* operation,
    OperationID* id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  *id = in_flight_operations_.Add(operation);
  DVLOG(1) << "GDataOperation[" << id << "] started.";
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnProgressUpdate(GetProgressStatusList()));
}

void GDataOperationRegistry::OnOperationProgress(OperationID id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Operation* operation = in_flight_operations_.Lookup(id);
  DCHECK(operation);

  DVLOG(1) << "GDataOperation[" << id << "] " <<
      operation->progress_status().ToString();
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnProgressUpdate(GetProgressStatusList()));
}

void GDataOperationRegistry::OnOperationFinish(OperationID id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DVLOG(1) << "GDataOperation[" << id << "] finished.";
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnProgressUpdate(GetProgressStatusList()));
  in_flight_operations_.Remove(id);
}

std::vector<GDataOperationRegistry::ProgressStatus>
GDataOperationRegistry::GetProgressStatusList() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::vector<ProgressStatus> status_list;
  for (OperationIDMap::const_iterator iter(&in_flight_operations_);
       !iter.IsAtEnd();
       iter.Advance()) {
    const Operation* operation = iter.GetCurrentValue();
    status_list.push_back(operation->progress_status());
  }
  return status_list;
}

}  // namespace gdata
