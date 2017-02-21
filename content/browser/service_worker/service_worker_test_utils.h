// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TEST_UTILS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TEST_UTILS_H_

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerDispatcherHost;
class ServiceWorkerProviderHost;

template <typename Arg>
void ReceiveResult(BrowserThread::ID run_quit_thread,
                   const base::Closure& quit,
                   Arg* out, Arg actual) {
  *out = actual;
  if (!quit.is_null())
    BrowserThread::PostTask(run_quit_thread, FROM_HERE, quit);
}

template <typename Arg> base::Callback<void(Arg)>
CreateReceiver(BrowserThread::ID run_quit_thread,
               const base::Closure& quit, Arg* out) {
  return base::Bind(&ReceiveResult<Arg>, run_quit_thread, quit, out);
}

template <typename Arg>
base::Callback<void(Arg)> CreateReceiverOnCurrentThread(
    Arg* out,
    const base::Closure& quit = base::Closure()) {
  BrowserThread::ID id;
  bool ret = BrowserThread::GetCurrentThreadIdentifier(&id);
  DCHECK(ret);
  return base::Bind(&ReceiveResult<Arg>, id, quit, out);
}

std::unique_ptr<ServiceWorkerProviderHost> CreateProviderHostForWindow(
    int process_id,
    int provider_id,
    bool is_parent_frame_secure,
    base::WeakPtr<ServiceWorkerContextCore> context);

std::unique_ptr<ServiceWorkerProviderHost>
CreateProviderHostForServiceWorkerContext(
    int process_id,
    int provider_id,
    bool is_parent_frame_secure,
    base::WeakPtr<ServiceWorkerContextCore> context);

std::unique_ptr<ServiceWorkerProviderHost> CreateProviderHostWithDispatcherHost(
    int process_id,
    int provider_id,
    base::WeakPtr<ServiceWorkerContextCore> context,
    int route_id,
    ServiceWorkerDispatcherHost* dispatcher_host);

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TEST_UTILS_H_
