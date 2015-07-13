// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/gcm_desktop_utils.h"

#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/common/gcm_desktop_util.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_driver.h"
#include "content/public/browser/browser_thread.h"

namespace gcm {

scoped_ptr<GCMDriver> CreateGCMDriverDesktop(
    scoped_ptr<GCMClientFactory> gcm_client_factory,
    PrefService* prefs,
    const base::FilePath& store_path,
    const scoped_refptr<net::URLRequestContextGetter>& request_context) {

  scoped_refptr<base::SequencedWorkerPool> worker_pool(
      content::BrowserThread::GetBlockingPool());
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner(
      worker_pool->GetSequencedTaskRunnerWithShutdownBehavior(
          worker_pool->GetSequenceToken(),
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));

  return CreateGCMDriverDesktopWithTaskRunners(
      gcm_client_factory.Pass(),
      prefs,
      store_path,
      request_context,
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::UI),
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::IO),
      blocking_task_runner);
}

}  // namespace gcm
