// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/push_messaging_service.h"

#include "base/callback.h"
#include "content/browser/push_messaging/push_messaging_message_filter.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"

namespace content {

namespace {

const char kNotificationsShownServiceWorkerKey[] =
    "notifications_shown_by_last_few_pushes";

void CallStringCallbackFromIO(
    const PushMessagingService::StringCallback& callback,
    const std::string& data,
    ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  bool success = service_worker_status == SERVICE_WORKER_OK;
  bool not_found = service_worker_status == SERVICE_WORKER_ERROR_NOT_FOUND;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, data, success, not_found));
}

void CallResultCallbackFromIO(
    const ServiceWorkerContext::ResultCallback& callback,
    ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  bool success = service_worker_status == SERVICE_WORKER_OK;
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, success));
}

void CallClosureFromIO(const base::Closure& callback,
                       ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback);
}

void GetUserDataOnIO(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_wrapper,
    int64_t service_worker_registration_id,
    const std::string& key,
    const PushMessagingService::StringCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  service_worker_context_wrapper->GetRegistrationUserData(
      service_worker_registration_id, key,
      base::Bind(&CallStringCallbackFromIO, callback));
}

void SetNotificationsShownOnIO(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_wrapper,
    int64_t service_worker_registration_id,
    const GURL& origin,
    const std::string& data,
    const PushMessagingService::ResultCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  service_worker_context_wrapper->StoreRegistrationUserData(
      service_worker_registration_id, origin,
      kNotificationsShownServiceWorkerKey, data,
      base::Bind(&CallResultCallbackFromIO, callback));
}

void ClearPushSubscriptionIDOnIO(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    int64_t service_worker_registration_id,
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context->ClearRegistrationUserData(
      service_worker_registration_id,
      kPushRegistrationIdServiceWorkerKey,
      base::Bind(&CallClosureFromIO, callback));
}

scoped_refptr<ServiceWorkerContextWrapper> GetServiceWorkerContext(
    BrowserContext* browser_context, const GURL& origin) {
  StoragePartition* partition =
      BrowserContext::GetStoragePartitionForSite(browser_context, origin);
  return make_scoped_refptr(
      static_cast<ServiceWorkerContextWrapper*>(
          partition->GetServiceWorkerContext()));
}

}  // anonymous namespace

// static
void PushMessagingService::GetNotificationsShownByLastFewPushes(
    ServiceWorkerContext* service_worker_context,
    int64_t service_worker_registration_id,
    const StringCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<ServiceWorkerContextWrapper> wrapper =
      static_cast<ServiceWorkerContextWrapper*>(service_worker_context);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&GetUserDataOnIO,
                                     wrapper,
                                     service_worker_registration_id,
                                     kNotificationsShownServiceWorkerKey,
                                     callback));
}

// static
void PushMessagingService::SetNotificationsShownByLastFewPushes(
    ServiceWorkerContext* service_worker_context,
    int64_t service_worker_registration_id,
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

// static
void PushMessagingService::GetSenderId(BrowserContext* browser_context,
                                       const GURL& origin,
                                       int64_t service_worker_registration_id,
                                       const StringCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GetUserDataOnIO,
                 GetServiceWorkerContext(browser_context, origin),
                 service_worker_registration_id,
                 kPushSenderIdServiceWorkerKey,
                 callback));
}

// static
void PushMessagingService::ClearPushSubscriptionID(
    BrowserContext* browser_context,
    const GURL& origin,
    int64_t service_worker_registration_id,
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ClearPushSubscriptionIDOnIO,
                 GetServiceWorkerContext(browser_context, origin),
                 service_worker_registration_id,
                 callback));
}

}  // namespace content
