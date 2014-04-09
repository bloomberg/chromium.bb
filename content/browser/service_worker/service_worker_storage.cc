// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_storage.h"

#include <string>
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/quota/quota_manager_proxy.h"

namespace content {

namespace {

void RunSoon(const base::Closure& closure) {
  base::MessageLoop::current()->PostTask(FROM_HERE, closure);
}

const base::FilePath::CharType kServiceWorkerDirectory[] =
    FILE_PATH_LITERAL("ServiceWorker");

}  // namespace

ServiceWorkerStorage::ServiceWorkerStorage(
    const base::FilePath& path,
    quota::QuotaManagerProxy* quota_manager_proxy)
    : last_registration_id_(0),  // TODO(kinuko): this should be read from disk.
      last_version_id_(0),       // TODO(kinuko): this should be read from disk.
      quota_manager_proxy_(quota_manager_proxy) {
  if (!path.empty())
    path_ = path.Append(kServiceWorkerDirectory);
}

ServiceWorkerStorage::~ServiceWorkerStorage() {
  registration_by_pattern_.clear();
}

void ServiceWorkerStorage::FindRegistrationForPattern(
    const GURL& pattern,
    const FindRegistrationCallback& callback) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_NOT_FOUND;
  scoped_refptr<ServiceWorkerRegistration> found;
  PatternToRegistrationMap::const_iterator match =
      registration_by_pattern_.find(pattern);
  if (match != registration_by_pattern_.end()) {
    status = SERVICE_WORKER_OK;
    found = match->second;
  }
  // Always simulate asynchronous call for now.
  RunSoon(base::Bind(callback, status, found));
}

void ServiceWorkerStorage::FindRegistrationForDocument(
    const GURL& document_url,
    const FindRegistrationCallback& callback) {
  // TODO(alecflett): This needs to be synchronous in the fast path,
  // but asynchronous in the slow path (when the patterns have to be
  // loaded from disk). For now it is always pessimistically async.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_NOT_FOUND;
  scoped_refptr<ServiceWorkerRegistration> found;
  for (PatternToRegistrationMap::const_iterator it =
           registration_by_pattern_.begin();
       it != registration_by_pattern_.end();
       ++it) {
    if (PatternMatches(it->first, document_url)) {
      status = SERVICE_WORKER_OK;
      found = it->second;
      break;
    }
  }
  // Always simulate asynchronous call for now.
  RunSoon(base::Bind(callback, status, found));
}

void ServiceWorkerStorage::GetAllRegistrations(
    const GetAllRegistrationInfosCallback& callback) {
  std::vector<ServiceWorkerRegistrationInfo> registrations;
  for (PatternToRegistrationMap::const_iterator it =
           registration_by_pattern_.begin();
       it != registration_by_pattern_.end();
       ++it) {
    ServiceWorkerRegistration* registration(it->second.get());
    registrations.push_back(registration->GetInfo());
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(callback, registrations));
}

void ServiceWorkerStorage::FindRegistrationForId(
    int64 registration_id,
    const FindRegistrationCallback& callback) {
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_NOT_FOUND;
  scoped_refptr<ServiceWorkerRegistration> found;
  for (PatternToRegistrationMap::const_iterator it =
           registration_by_pattern_.begin();
       it != registration_by_pattern_.end();
       ++it) {
    if (registration_id == it->second->id()) {
      status = SERVICE_WORKER_OK;
      found = it->second;
      break;
    }
  }
  RunSoon(base::Bind(callback, status, found));
}

void ServiceWorkerStorage::StoreRegistration(
    ServiceWorkerRegistration* registration,
    const StatusCallback& callback) {
  DCHECK(registration);

  PatternToRegistrationMap::const_iterator current(
      registration_by_pattern_.find(registration->pattern()));
  if (current != registration_by_pattern_.end() &&
      current->second->script_url() != registration->script_url()) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_EXISTS));
    return;
  }

  // This may update the existing registration information.
  registration_by_pattern_[registration->pattern()] = registration;

  RunSoon(base::Bind(callback, SERVICE_WORKER_OK));
}

void ServiceWorkerStorage::DeleteRegistration(
    const GURL& pattern,
    const StatusCallback& callback) {
  PatternToRegistrationMap::iterator match =
      registration_by_pattern_.find(pattern);
  if (match == registration_by_pattern_.end()) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_NOT_FOUND));
    return;
  }
  registration_by_pattern_.erase(match);
  RunSoon(base::Bind(callback, SERVICE_WORKER_OK));
}

int64 ServiceWorkerStorage::NewRegistrationId() {
  return ++last_registration_id_;
}

int64 ServiceWorkerStorage::NewVersionId() {
  return ++last_version_id_;
}

bool ServiceWorkerStorage::PatternMatches(const GURL& pattern,
                                          const GURL& url) {
  // This is a really basic, naive
  // TODO(alecflett): Formalize what pattern matches mean.
  // Temporarily borrowed directly from appcache::Namespace::IsMatch().
  // We have to escape '?' characters since MatchPattern also treats those
  // as wildcards which we don't want here, we only do '*'s.
  std::string pattern_spec(pattern.spec());
  if (pattern.has_query())
    ReplaceSubstringsAfterOffset(&pattern_spec, 0, "?", "\\?");
  return MatchPattern(url.spec(), pattern_spec);
}

}  // namespace content
