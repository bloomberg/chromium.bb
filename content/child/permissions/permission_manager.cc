// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/permissions/permission_manager.h"

#include "content/child/worker_task_runner.h"
#include "content/public/common/service_registry.h"
#include "third_party/WebKit/public/platform/WebURL.h"

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
    default:
      // The default statement is only there to prevent compilation failures if
      // WebPermissionType enum gets extended.
      NOTREACHED();
      return PERMISSION_NAME_GEOLOCATION;
  }
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

PermissionManager::CallbackInformation::CallbackInformation(
    blink::WebPermissionQueryCallback* callback,
    int worker_thread_id)
  : callback_(callback),
    worker_thread_id_(worker_thread_id) {
}

blink::WebPermissionQueryCallback*
PermissionManager::CallbackInformation::callback() const {
  return callback_.get();
}

int PermissionManager::CallbackInformation::worker_thread_id() const {
  return worker_thread_id_;
}

blink::WebPermissionQueryCallback*
PermissionManager::CallbackInformation::ReleaseCallback() {
  return callback_.release();
}

PermissionManager::CallbackInformation::~CallbackInformation() {
}

PermissionManager::PermissionManager(ServiceRegistry* service_registry)
    : service_registry_(service_registry) {
}

PermissionManager::~PermissionManager() {
}

void PermissionManager::queryPermission(
    blink::WebPermissionType type,
    const blink::WebURL& origin,
    blink::WebPermissionQueryCallback* callback) {
  QueryPermissionInternal(
      type, origin.string().utf8(), callback, kNoWorkerThread);
}

void PermissionManager::QueryPermissionForWorker(
    blink::WebPermissionType type,
    const std::string& origin,
    blink::WebPermissionQueryCallback* callback,
    int worker_thread_id) {
  QueryPermissionInternal(type, origin, callback, worker_thread_id);
}

void PermissionManager::QueryPermissionInternal(
    blink::WebPermissionType type,
    const std::string& origin,
    blink::WebPermissionQueryCallback* callback,
    int worker_thread_id) {
  // We need to save the |callback| in an IDMap so if |this| gets deleted, the
  // callback will not leak. In the case of |this| gets deleted, the
  // |permission_service_| pipe will be destroyed too so OnQueryPermission will
  // not be called.
  int request_id = pending_callbacks_.Add(
      new CallbackInformation(callback, worker_thread_id));
  if (!permission_service_.get())
    service_registry_->ConnectToRemoteService(&permission_service_);
  permission_service_->HasPermission(
      GetPermissionName(type),
      origin,
      base::Bind(&PermissionManager::OnQueryPermission,
                 base::Unretained(this),
                 request_id));
}

void PermissionManager::OnQueryPermission(int request_id,
                                           PermissionStatus result) {
  CallbackInformation* callback_information =
      pending_callbacks_.Lookup(request_id);
  DCHECK(callback_information && callback_information->callback());
  scoped_ptr<blink::WebPermissionStatus> status(
      new blink::WebPermissionStatus(GetWebPermissionStatus(result)));

  if (callback_information->worker_thread_id() != kNoWorkerThread) {
    blink::WebPermissionQueryCallback* callback =
        callback_information->ReleaseCallback();
    int worker_thread_id = callback_information->worker_thread_id();
    pending_callbacks_.Remove(request_id);

    // If the worker is no longer running, ::PostTask() will return false and
    // gracefully fail, destroying the callback too.
    WorkerTaskRunner::Instance()->PostTask(
        worker_thread_id,
        base::Bind(&PermissionManager::RunCallbackOnWorkerThread,
                   base::Unretained(callback),
                   base::Passed(&status)));
    return;
  }

  callback_information->callback()->onSuccess(status.release());
  pending_callbacks_.Remove(request_id);
}

// static
void PermissionManager::RunCallbackOnWorkerThread(
    blink::WebPermissionQueryCallback* callback,
    scoped_ptr<blink::WebPermissionStatus> status) {
  callback->onSuccess(status.release());
  delete callback;
}

}  // namespace content
