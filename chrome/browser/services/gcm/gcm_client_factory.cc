// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/gcm_client_factory.h"

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gcm/gcm_client_impl.h"

namespace gcm {

namespace {

static base::LazyInstance<GCMClientImpl>::Leaky g_gcm_client =
    LAZY_INSTANCE_INITIALIZER;
static bool g_gcm_client_initialized = false;
static GCMClientFactory::TestingFactoryFunction g_gcm_client_factory = NULL;
static GCMClient* g_gcm_client_override = NULL;

}  // namespace


// static
GCMClient* GCMClientFactory::GetClient() {
  if (g_gcm_client_override)
    return g_gcm_client_override;
  if (g_gcm_client_factory) {
    g_gcm_client_override = g_gcm_client_factory();
    return g_gcm_client_override;
  }

  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  GCMClientImpl* client = g_gcm_client.Pointer();
  if (!g_gcm_client_initialized) {
    // TODO(jianli): get gcm store path.
    base::FilePath gcm_store_path;
    scoped_refptr<base::SequencedWorkerPool> worker_pool(
        content::BrowserThread::GetBlockingPool());
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner(
        worker_pool->GetSequencedTaskRunnerWithShutdownBehavior(
            worker_pool->GetSequenceToken(),
            base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));
    // TODO(jianli): proper initialization in progress.
    // client->Initialize(gcm_store_path, blocking_task_runner);
    g_gcm_client_initialized = true;
  }
  return client;
}

// static
void GCMClientFactory::SetTestingFactory(TestingFactoryFunction factory) {
  if (g_gcm_client_override) {
    delete g_gcm_client_override;
    g_gcm_client_override = NULL;
  }
  g_gcm_client_factory = factory;
}

GCMClientFactory::GCMClientFactory() {
}

GCMClientFactory::~GCMClientFactory() {
}

}  // namespace gcm
