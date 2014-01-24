// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_storage.h"

#include <string>

#include "base/strings/string_util.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/quota/quota_manager_proxy.h"

namespace {
// This is temporary until we figure out how registration ids will be
// calculated.
int64 NextRegistrationId() {
  static int64 worker_id = 0;
  return worker_id++;
}
}  // namespace

namespace content {

const base::FilePath::CharType kServiceWorkerDirectory[] =
    FILE_PATH_LITERAL("ServiceWorker");

ServiceWorkerStorage::ServiceWorkerStorage(
    const base::FilePath& path,
    quota::QuotaManagerProxy* quota_manager_proxy)
    : quota_manager_proxy_(quota_manager_proxy) {
  if (!path.empty())
    path_ = path.Append(kServiceWorkerDirectory);
}

ServiceWorkerStorage::~ServiceWorkerStorage() {
  for (PatternToRegistrationMap::const_iterator iter =
           registration_by_pattern_.begin();
       iter != registration_by_pattern_.end();
       ++iter) {
    iter->second->Shutdown();
  }
  registration_by_pattern_.clear();
}

void ServiceWorkerStorage::FindRegistrationForPattern(
    const GURL& pattern,
    const FindRegistrationCallback& callback) {
  PatternToRegistrationMap::const_iterator match =
      registration_by_pattern_.find(pattern);
  if (match == registration_by_pattern_.end()) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(callback,
                   false /* found */,
                   SERVICE_WORKER_OK,
                   scoped_refptr<ServiceWorkerRegistration>()));
    return;
  }
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(callback, true /* found */, SERVICE_WORKER_OK, match->second));
}

void ServiceWorkerStorage::FindRegistrationForDocument(
    const GURL& document_url,
    const FindRegistrationCallback& callback) {
  // TODO(alecflett): This needs to be synchronous in the fast path,
  // but asynchronous in the slow path (when the patterns have to be
  // loaded from disk). For now it is always pessimistically async.
  for (PatternToRegistrationMap::const_iterator it =
           registration_by_pattern_.begin();
       it != registration_by_pattern_.end();
       ++it) {
    if (PatternMatches(it->first, document_url)) {
      BrowserThread::PostTask(
          BrowserThread::IO,
          FROM_HERE,
          base::Bind(callback,
                     true /* found */,
                     SERVICE_WORKER_OK,
                     scoped_refptr<ServiceWorkerRegistration>(it->second)));
      return;
    }
  }
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(callback,
                 false /* found */,
                 SERVICE_WORKER_OK,
                 scoped_refptr<ServiceWorkerRegistration>()));
}

scoped_refptr<ServiceWorkerRegistration> ServiceWorkerStorage::RegisterInternal(
    const GURL& pattern,
    const GURL& script_url) {

  PatternToRegistrationMap::const_iterator current(
      registration_by_pattern_.find(pattern));
  DCHECK(current == registration_by_pattern_.end() ||
         current->second->script_url() == script_url);

  if (current == registration_by_pattern_.end()) {
    scoped_refptr<ServiceWorkerRegistration> registration(
        new ServiceWorkerRegistration(
            pattern, script_url, NextRegistrationId()));
    // TODO(alecflett): version upgrade path.
    registration_by_pattern_[pattern] = registration;
    return registration;
  }

  return current->second;
}

void ServiceWorkerStorage::UnregisterInternal(const GURL& pattern) {
  PatternToRegistrationMap::iterator match =
      registration_by_pattern_.find(pattern);
  if (match != registration_by_pattern_.end()) {
    match->second->Shutdown();
    registration_by_pattern_.erase(match);
  }
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
