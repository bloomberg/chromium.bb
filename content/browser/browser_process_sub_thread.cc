// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_process_sub_thread.h"

#if defined(OS_WIN)
#include <Objbase.h>
#endif

#include "base/debug/leak_tracker.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/in_process_webkit/indexed_db_key_utility_client.h"
#include "content/browser/notification_service_impl.h"
#include "content/public/common/url_fetcher.h"
#include "net/url_request/url_request.h"

namespace content {

BrowserProcessSubThread::BrowserProcessSubThread(BrowserThread::ID identifier)
    : BrowserThreadImpl(identifier),
      notification_service_(NULL) {
}

BrowserProcessSubThread::~BrowserProcessSubThread() {
  Stop();
}

void BrowserProcessSubThread::Init() {
#if defined(OS_WIN)
  // Initializes the COM library on the current thread.
  CoInitialize(NULL);
#endif

  notification_service_ = new NotificationServiceImpl;

  BrowserThreadImpl::Init();

  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    // Though this thread is called the "IO" thread, it actually just routes
    // messages around; it shouldn't be allowed to perform any blocking disk
    // I/O.
    base::ThreadRestrictions::SetIOAllowed(false);
    base::ThreadRestrictions::DisallowWaiting();
  }
}

void BrowserProcessSubThread::CleanUp() {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO))
    IOThreadPreCleanUp();

  BrowserThreadImpl::CleanUp();

  delete notification_service_;
  notification_service_ = NULL;

#if defined(OS_WIN)
  // Closes the COM library on the current thread. CoInitialize must
  // be balanced by a corresponding call to CoUninitialize.
  CoUninitialize();
#endif
}

void BrowserProcessSubThread::IOThreadPreCleanUp() {
  // Kill all things that might be holding onto
  // net::URLRequest/net::URLRequestContexts.

  // Destroy all URLRequests started by URLFetchers.
  content::URLFetcher::CancelAll();

  IndexedDBKeyUtilityClient::Shutdown();

  // If any child processes are still running, terminate them and
  // and delete the BrowserChildProcessHost instances to release whatever
  // IO thread only resources they are referencing.
  BrowserChildProcessHostImpl::TerminateAll();
}

}  // namespace content
