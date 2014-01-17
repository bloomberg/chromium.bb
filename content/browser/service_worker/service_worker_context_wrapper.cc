// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_wrapper.h"

#include "base/files/file_path.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/quota/quota_manager_proxy.h"

namespace content {

ServiceWorkerContextWrapper::ServiceWorkerContextWrapper() {
}

ServiceWorkerContextWrapper::~ServiceWorkerContextWrapper() {
}

void ServiceWorkerContextWrapper::Init(
    const base::FilePath& user_data_directory,
    quota::QuotaManagerProxy* quota_manager_proxy) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::Init, this,
                   user_data_directory,
                   make_scoped_refptr(quota_manager_proxy)));
    return;
  }
  DCHECK(!context_core_);
  context_core_.reset(
      new ServiceWorkerContextCore(
          user_data_directory, quota_manager_proxy));
}

void ServiceWorkerContextWrapper::Shutdown() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::Shutdown, this));
    return;
  }
  context_core_.reset();
}

ServiceWorkerContextCore* ServiceWorkerContextWrapper::context() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return context_core_.get();
}

}  // namespace content
