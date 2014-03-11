// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registration.h"

#include "content/public/browser/browser_thread.h"

namespace content {

ServiceWorkerRegistration::ServiceWorkerRegistration(const GURL& pattern,
                                                     const GURL& script_url,
                                                     int64 registration_id)
    : pattern_(pattern),
      script_url_(script_url),
      registration_id_(registration_id),
      is_shutdown_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

ServiceWorkerRegistration::~ServiceWorkerRegistration() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(is_shutdown_);
}

void ServiceWorkerRegistration::Shutdown() {
  DCHECK(!is_shutdown_);
  if (active_version_)
    active_version_->Shutdown();
  active_version_ = NULL;
  if (pending_version_)
    pending_version_->Shutdown();
  pending_version_ = NULL;
  is_shutdown_ = true;
}

void ServiceWorkerRegistration::ActivatePendingVersion() {
  active_version_->set_status(ServiceWorkerVersion::DEACTIVATED);
  active_version_->Shutdown();
  active_version_ = pending_version_;
  active_version_->set_status(ServiceWorkerVersion::ACTIVE);
  pending_version_ = NULL;
}

}  // namespace content
