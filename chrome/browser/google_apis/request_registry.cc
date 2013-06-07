// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/request_registry.h"

#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace google_apis {

RequestProgressStatus::RequestProgressStatus(const base::FilePath& path)
    : request_id(-1),
      file_path(path),
      transfer_state(REQUEST_NOT_STARTED) {
}

RequestRegistry::Request::Request(RequestRegistry* registry)
    : registry_(registry),
      progress_status_(base::FilePath()) {
}

RequestRegistry::Request::Request(RequestRegistry* registry,
                                        const base::FilePath& path)
    : registry_(registry),
      progress_status_(path) {
}

RequestRegistry::Request::~Request() {
  DCHECK(progress_status_.transfer_state == REQUEST_COMPLETED ||
         progress_status_.transfer_state == REQUEST_SUSPENDED ||
         progress_status_.transfer_state == REQUEST_FAILED);
}

void RequestRegistry::Request::Cancel() {
  DoCancel();
  NotifyFinish(REQUEST_FAILED);
}

void RequestRegistry::Request::NotifyStart() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Some request_ids may be restarted. Report only the first "start".
  if (progress_status_.transfer_state == REQUEST_NOT_STARTED) {
    progress_status_.transfer_state = REQUEST_STARTED;
    registry_->OnRequestStart(this, &progress_status_.request_id);
  }
}

void RequestRegistry::Request::NotifyFinish(
    RequestTransferState status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(progress_status_.transfer_state >= REQUEST_STARTED);
  DCHECK(status == REQUEST_COMPLETED || status == REQUEST_FAILED);
  progress_status_.transfer_state = status;
  registry_->OnRequestFinish(progress_status().request_id);
}

void RequestRegistry::Request::NotifySuspend() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(progress_status_.transfer_state >= REQUEST_STARTED);
  progress_status_.transfer_state = REQUEST_SUSPENDED;
  registry_->OnRequestSuspend(progress_status().request_id);
}

void RequestRegistry::Request::NotifyResume() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (progress_status_.transfer_state == REQUEST_NOT_STARTED) {
    progress_status_.transfer_state = REQUEST_IN_PROGRESS;
    registry_->OnRequestResume(this, &progress_status_);
  }
}

RequestRegistry::RequestRegistry() {
  in_flight_requests_.set_check_on_null_data(true);
}

RequestRegistry::~RequestRegistry() {
  DCHECK(in_flight_requests_.IsEmpty());
}

void RequestRegistry::CancelAll() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (RequestIDMap::iterator iter(&in_flight_requests_);
       !iter.IsAtEnd();
       iter.Advance()) {
    Request* request = iter.GetCurrentValue();
    CancelRequest(request);
    // CancelRequest may immediately trigger OnRequestFinish and remove the
    // request from the map, but IDMap is designed to be safe on such remove
    // while iteration.
  }
}

bool RequestRegistry::CancelForFilePath(const base::FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (RequestIDMap::iterator iter(&in_flight_requests_);
       !iter.IsAtEnd();
       iter.Advance()) {
    Request* request = iter.GetCurrentValue();
    if (request->progress_status().file_path == file_path) {
      CancelRequest(request);
      return true;
    }
  }
  return false;
}

void RequestRegistry::CancelRequest(Request* request) {
  if (request->progress_status().transfer_state == REQUEST_SUSPENDED) {
    // SUSPENDED request already completed its job (like calling back to
    // its client code). Invoking request->Cancel() again on it is a kind of
    // 'double deletion'. So here we directly call OnRequestFinish and just
    // unregister the request from the registry.
    // TODO(kinaba): http://crbug.com/164098 Get rid of the hack.
    OnRequestFinish(request->progress_status().request_id);
  } else {
    request->Cancel();
  }
}

void RequestRegistry::OnRequestStart(
    RequestRegistry::Request* request,
    RequestID* id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  *id = in_flight_requests_.Add(request);
  DVLOG(1) << "Request[" << *id << "] started.";
}

void RequestRegistry::OnRequestFinish(RequestID id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Request* request = in_flight_requests_.Lookup(id);
  DCHECK(request);

  DVLOG(1) << "Request[" << id << "] finished.";
  in_flight_requests_.Remove(id);
}

void RequestRegistry::OnRequestResume(
    RequestRegistry::Request* request,
    RequestProgressStatus* new_status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Find the corresponding suspended task.
  Request* suspended = NULL;
  for (RequestIDMap::iterator iter(&in_flight_requests_);
       !iter.IsAtEnd();
       iter.Advance()) {
    Request* in_flight_request = iter.GetCurrentValue();
    const RequestProgressStatus& status =
        in_flight_request->progress_status();
    if (status.transfer_state == REQUEST_SUSPENDED &&
        status.file_path == request->progress_status().file_path) {
      suspended = in_flight_request;
      break;
    }
  }

  if (!suspended) {
    // Preceding suspended requests was not found. Assume it was canceled.
    //
    // request->Cancel() needs to be called to properly shut down the
    // current request, but request->Cancel() tries to unregister itself
    // from the registry. So, as a hack, temporarily assign it an ID.
    // TODO(kinaba): http://crbug.com/164098 Get rid of it.
    new_status->request_id = in_flight_requests_.Add(request);
    CancelRequest(request);
    return;
  }

  // Remove the old one and initiate the new request.
  const RequestProgressStatus& old_status = suspended->progress_status();
  RequestID old_id = old_status.request_id;
  in_flight_requests_.Remove(old_id);
  new_status->request_id = in_flight_requests_.Add(request);
  DVLOG(1) << "Request[" << old_id << " -> " <<
           new_status->request_id << "] resumed.";
}

void RequestRegistry::OnRequestSuspend(RequestID id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Request* request = in_flight_requests_.Lookup(id);
  DCHECK(request);

  DVLOG(1) << "Request[" << id << "] suspended.";
}

}  // namespace google_apis
