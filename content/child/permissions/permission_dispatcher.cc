// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/permissions/permission_dispatcher.h"

#include "base/callback.h"
#include "content/child/worker_task_runner.h"
#include "content/public/common/service_registry.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/modules/permissions/WebPermissionObserver.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"

using blink::WebPermissionObserver;

namespace content {

namespace {

PermissionName GetPermissionName(blink::WebPermissionType type) {
  switch (type) {
    case blink::WebPermissionTypeGeolocation:
      return PERMISSION_NAME_GEOLOCATION;
    case blink::WebPermissionTypeNotifications:
      return PERMISSION_NAME_NOTIFICATIONS;
    case blink::WebPermissionTypePushNotifications:
      return PERMISSION_NAME_PUSH_NOTIFICATIONS;
    case blink::WebPermissionTypeMidiSysEx:
      return PERMISSION_NAME_MIDI_SYSEX;
    case blink::WebPermissionTypeMidi:
      return PERMISSION_NAME_MIDI;
    default:
      // The default statement is only there to prevent compilation failures if
      // WebPermissionType enum gets extended.
      NOTREACHED();
      return PERMISSION_NAME_GEOLOCATION;
  }
}

PermissionStatus GetPermissionStatus(blink::WebPermissionStatus status) {
  switch (status) {
    case blink::WebPermissionStatusGranted:
      return PERMISSION_STATUS_GRANTED;
    case blink::WebPermissionStatusDenied:
      return PERMISSION_STATUS_DENIED;
    case blink::WebPermissionStatusPrompt:
      return PERMISSION_STATUS_ASK;
  }

  NOTREACHED();
  return PERMISSION_STATUS_DENIED;
}

blink::WebPermissionStatus GetWebPermissionStatus(PermissionStatus status) {
  switch (status) {
    case PERMISSION_STATUS_GRANTED:
      return blink::WebPermissionStatusGranted;
    case PERMISSION_STATUS_DENIED:
      return blink::WebPermissionStatusDenied;
    case PERMISSION_STATUS_ASK:
      return blink::WebPermissionStatusPrompt;
  }

  NOTREACHED();
  return blink::WebPermissionStatusDenied;
}

const int kNoWorkerThread = 0;

}  // anonymous namespace

// static
bool PermissionDispatcher::IsObservable(blink::WebPermissionType type) {
  return type == blink::WebPermissionTypeGeolocation ||
         type == blink::WebPermissionTypeNotifications ||
         type == blink::WebPermissionTypePushNotifications ||
         type == blink::WebPermissionTypeMidiSysEx;
}

PermissionDispatcher::CallbackInformation::CallbackInformation(
    blink::WebPermissionCallback* callback,
    int worker_thread_id)
  : callback_(callback),
    worker_thread_id_(worker_thread_id) {
}

blink::WebPermissionCallback*
PermissionDispatcher::CallbackInformation::callback() const {
  return callback_.get();
}

int PermissionDispatcher::CallbackInformation::worker_thread_id() const {
  return worker_thread_id_;
}

blink::WebPermissionCallback*
PermissionDispatcher::CallbackInformation::ReleaseCallback() {
  return callback_.release();
}

PermissionDispatcher::CallbackInformation::~CallbackInformation() {
}

PermissionDispatcher::PermissionDispatcher(ServiceRegistry* service_registry)
    : service_registry_(service_registry) {
}

PermissionDispatcher::~PermissionDispatcher() {
}

void PermissionDispatcher::queryPermission(
    blink::WebPermissionType type,
    const blink::WebURL& origin,
    blink::WebPermissionCallback* callback) {
  QueryPermissionInternal(
      type, origin.string().utf8(), callback, kNoWorkerThread);
}

void PermissionDispatcher::requestPermission(
    blink::WebPermissionType type,
    const blink::WebURL& origin,
    blink::WebPermissionCallback* callback) {
  RequestPermissionInternal(
      type, origin.string().utf8(), callback, kNoWorkerThread);
}

void PermissionDispatcher::revokePermission(
    blink::WebPermissionType type,
    const blink::WebURL& origin,
    blink::WebPermissionCallback* callback) {
  RevokePermissionInternal(
      type, origin.string().utf8(), callback, kNoWorkerThread);
}

void PermissionDispatcher::startListening(
    blink::WebPermissionType type,
    const blink::WebURL& origin,
    WebPermissionObserver* observer) {
  if (!IsObservable(type))
    return;

  RegisterObserver(observer);

  GetNextPermissionChange(type,
                          origin.string().utf8(),
                          observer,
                          // We initialize with an arbitrary value because the
                          // mojo service wants a value. Worst case, the
                          // observer will get notified about a non-change which
                          // should be a no-op. After the first notification,
                          // GetNextPermissionChange will be called with the
                          // latest known value.
                          PERMISSION_STATUS_ASK);
}

void PermissionDispatcher::stopListening(WebPermissionObserver* observer) {
  UnregisterObserver(observer);
}

void PermissionDispatcher::QueryPermissionForWorker(
    blink::WebPermissionType type,
    const std::string& origin,
    blink::WebPermissionCallback* callback,
    int worker_thread_id) {
  QueryPermissionInternal(type, origin, callback, worker_thread_id);
}

void PermissionDispatcher::RequestPermissionForWorker(
    blink::WebPermissionType type,
    const std::string& origin,
    blink::WebPermissionCallback* callback,
    int worker_thread_id) {
  RequestPermissionInternal(type, origin, callback, worker_thread_id);
}

void PermissionDispatcher::RevokePermissionForWorker(
    blink::WebPermissionType type,
    const std::string& origin,
    blink::WebPermissionCallback* callback,
    int worker_thread_id) {
  RevokePermissionInternal(type, origin, callback, worker_thread_id);
}

void PermissionDispatcher::StartListeningForWorker(
    blink::WebPermissionType type,
    const std::string& origin,
    int worker_thread_id,
    const base::Callback<void(blink::WebPermissionStatus)>& callback) {
  GetPermissionServicePtr()->GetNextPermissionChange(
      GetPermissionName(type),
      origin,
      // We initialize with an arbitrary value because the mojo service wants a
      // value. Worst case, the observer will get notified about a non-change
      // which should be a no-op. After the first notification,
      // GetNextPermissionChange will be called with the latest known value.
      PERMISSION_STATUS_ASK,
      base::Bind(&PermissionDispatcher::OnPermissionChangedForWorker,
                 base::Unretained(this),
                 worker_thread_id,
                 callback));
}

void PermissionDispatcher::GetNextPermissionChangeForWorker(
    blink::WebPermissionType type,
    const std::string& origin,
    blink::WebPermissionStatus status,
    int worker_thread_id,
    const base::Callback<void(blink::WebPermissionStatus)>& callback) {
  GetPermissionServicePtr()->GetNextPermissionChange(
      GetPermissionName(type),
      origin,
      GetPermissionStatus(status),
      base::Bind(&PermissionDispatcher::OnPermissionChangedForWorker,
                 base::Unretained(this),
                 worker_thread_id,
                 callback));
}

// static
void PermissionDispatcher::RunCallbackOnWorkerThread(
    blink::WebPermissionCallback* callback,
    scoped_ptr<blink::WebPermissionStatus> status) {
  callback->onSuccess(status.release());
  delete callback;
}

PermissionServicePtr& PermissionDispatcher::GetPermissionServicePtr() {
  if (!permission_service_.get()) {
    service_registry_->ConnectToRemoteService(
        mojo::GetProxy(&permission_service_));
  }
  return permission_service_;
}

void PermissionDispatcher::QueryPermissionInternal(
    blink::WebPermissionType type,
    const std::string& origin,
    blink::WebPermissionCallback* callback,
    int worker_thread_id) {
  // We need to save the |callback| in an IDMap so if |this| gets deleted, the
  // callback will not leak. In the case of |this| gets deleted, the
  // |permission_service_| pipe will be destroyed too so OnQueryPermission will
  // not be called.
  int request_id = pending_callbacks_.Add(
      new CallbackInformation(callback, worker_thread_id));
  GetPermissionServicePtr()->HasPermission(
      GetPermissionName(type),
      origin,
      base::Bind(&PermissionDispatcher::OnPermissionResponse,
                 base::Unretained(this),
                 request_id));
}

void PermissionDispatcher::RequestPermissionInternal(
    blink::WebPermissionType type,
    const std::string& origin,
    blink::WebPermissionCallback* callback,
    int worker_thread_id) {
  // We need to save the |callback| in an IDMap so if |this| gets deleted, the
  // callback will not leak. In the case of |this| gets deleted, the
  // |permission_service_| pipe will be destroyed too so OnQueryPermission will
  // not be called.
  int request_id = pending_callbacks_.Add(
      new CallbackInformation(callback, worker_thread_id));
  GetPermissionServicePtr()->RequestPermission(
      GetPermissionName(type),
      origin,
      blink::WebUserGestureIndicator::isProcessingUserGesture(),
      base::Bind(&PermissionDispatcher::OnPermissionResponse,
                 base::Unretained(this),
                 request_id));
}

void PermissionDispatcher::RevokePermissionInternal(
    blink::WebPermissionType type,
    const std::string& origin,
    blink::WebPermissionCallback* callback,
    int worker_thread_id) {
  // We need to save the |callback| in an IDMap so if |this| gets deleted, the
  // callback will not leak. In the case of |this| gets deleted, the
  // |permission_service_| pipe will be destroyed too so OnQueryPermission will
  // not be called.
  int request_id = pending_callbacks_.Add(
      new CallbackInformation(callback, worker_thread_id));
  GetPermissionServicePtr()->RevokePermission(
      GetPermissionName(type),
      origin,
      base::Bind(&PermissionDispatcher::OnPermissionResponse,
                 base::Unretained(this),
                 request_id));
}

void PermissionDispatcher::OnPermissionResponse(int request_id,
                                                PermissionStatus result) {
  CallbackInformation* callback_information =
      pending_callbacks_.Lookup(request_id);
  DCHECK(callback_information && callback_information->callback());
  scoped_ptr<blink::WebPermissionStatus> status(
      new blink::WebPermissionStatus(GetWebPermissionStatus(result)));

  if (callback_information->worker_thread_id() != kNoWorkerThread) {
    blink::WebPermissionCallback* callback =
        callback_information->ReleaseCallback();
    int worker_thread_id = callback_information->worker_thread_id();
    pending_callbacks_.Remove(request_id);

    // If the worker is no longer running, ::PostTask() will return false and
    // gracefully fail, destroying the callback too.
    WorkerTaskRunner::Instance()->PostTask(
        worker_thread_id,
        base::Bind(&PermissionDispatcher::RunCallbackOnWorkerThread,
                   base::Unretained(callback),
                   base::Passed(&status)));
    return;
  }

  callback_information->callback()->onSuccess(status.release());
  pending_callbacks_.Remove(request_id);
}

void PermissionDispatcher::OnPermissionChanged(
    blink::WebPermissionType type,
    const std::string& origin,
    WebPermissionObserver* observer,
    PermissionStatus status) {
  if (!IsObserverRegistered(observer))
    return;

  observer->permissionChanged(type, GetWebPermissionStatus(status));

  GetNextPermissionChange(type, origin, observer, status);
}

void PermissionDispatcher::OnPermissionChangedForWorker(
    int worker_thread_id,
    const base::Callback<void(blink::WebPermissionStatus)>& callback,
    PermissionStatus status) {
  DCHECK(worker_thread_id != kNoWorkerThread);

  WorkerTaskRunner::Instance()->PostTask(
      worker_thread_id, base::Bind(callback, GetWebPermissionStatus(status)));
}

void PermissionDispatcher::GetNextPermissionChange(
    blink::WebPermissionType type,
    const std::string& origin,
    WebPermissionObserver* observer,
    PermissionStatus current_status) {
  GetPermissionServicePtr()->GetNextPermissionChange(
      GetPermissionName(type),
      origin,
      current_status,
      base::Bind(&PermissionDispatcher::OnPermissionChanged,
                 base::Unretained(this),
                 type,
                 origin,
                 base::Unretained(observer)));
}

}  // namespace content
