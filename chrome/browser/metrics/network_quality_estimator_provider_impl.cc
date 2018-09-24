// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/network_quality_estimator_provider_impl.h"

#include "base/sequenced_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "content/public/browser/browser_thread.h"

namespace metrics {

NetworkQualityEstimatorProviderImpl::NetworkQualityEstimatorProviderImpl()
    : weak_ptr_factory_(this) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

NetworkQualityEstimatorProviderImpl::~NetworkQualityEstimatorProviderImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void NetworkQualityEstimatorProviderImpl::PostReplyNetworkQualityTracker(
    base::OnceCallback<void(network::NetworkQualityTracker*)> callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!content::BrowserThread::IsThreadInitialized(
          content::BrowserThread::IO)) {
    // IO thread is not yet initialized. Try again in the next message pump.
    bool task_posted = base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&NetworkQualityEstimatorProviderImpl::
                           PostReplyNetworkQualityTracker,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    DCHECK(task_posted);
    return;
  }

  std::move(callback).Run(g_browser_process->network_quality_tracker());
}

}  // namespace metrics
