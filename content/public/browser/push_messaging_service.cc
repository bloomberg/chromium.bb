// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/push_messaging_service.h"

#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"

namespace {

const char kNotificationsShownServiceWorkerKey[] =
    "notifications_shown_by_last_few_pushes";

}  // namespace

namespace content {

static void CallGetNotificationsShownCallbackFromIO(
    const PushMessagingService::GetNotificationsShownCallback& callback,
    const std::string& data,
    ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  bool success = service_worker_status == SERVICE_WORKER_OK;
  bool not_found = service_worker_status == SERVICE_WORKER_ERROR_NOT_FOUND;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, data, success, not_found));
}

static void CallResultCallbackFromIO(
    const ServiceWorkerContext::ResultCallback& callback,
    ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  bool success = service_worker_status == SERVICE_WORKER_OK;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, success));
}

static void GetNotificationsShownOnIO(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_wrapper,
    int64 service_worker_registration_id,
    const PushMessagingService::GetNotificationsShownCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  service_worker_context_wrapper->context()->storage()->GetUserData(
      service_worker_registration_id, kNotificationsShownServiceWorkerKey,
      base::Bind(&CallGetNotificationsShownCallbackFromIO, callback));
}

static void SetNotificationsShownOnIO(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_wrapper,
    int64 service_worker_registration_id, const GURL& origin,
    const std::string& data,
    const PushMessagingService::ResultCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  service_worker_context_wrapper->context()->storage()->StoreUserData(
      service_worker_registration_id, origin,
      kNotificationsShownServiceWorkerKey, data,
      base::Bind(&CallResultCallbackFromIO, callback));
}

// static
void PushMessagingService::GetNotificationsShownByLastFewPushes(
    ServiceWorkerContext* service_worker_context,
    int64 service_worker_registration_id,
    const GetNotificationsShownCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<ServiceWorkerContextWrapper> wrapper =
      static_cast<ServiceWorkerContextWrapper*>(service_worker_context);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&GetNotificationsShownOnIO,
                                     wrapper,
                                     service_worker_registration_id,
                                     callback));
}

// static
void PushMessagingService::SetNotificationsShownByLastFewPushes(
    ServiceWorkerContext* service_worker_context,
    int64 service_worker_registration_id,
    const GURL& origin,
    const std::string& notifications_shown,
    const ResultCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<ServiceWorkerContextWrapper> wrapper =
      static_cast<ServiceWorkerContextWrapper*>(service_worker_context);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&SetNotificationsShownOnIO,
                                     wrapper,
                                     service_worker_registration_id,
                                     origin,
                                     notifications_shown,
                                     callback));
}

}  // namespace content
