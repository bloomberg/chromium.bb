// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/network_quality_estimator_provider_impl.h"

#include "chrome/browser/io_thread.h"
#include "content/public/browser/browser_thread.h"

namespace net {
class NetworkQualityEstimator;
}

namespace metrics {

NetworkQualityEstimatorProviderImpl::NetworkQualityEstimatorProviderImpl(
    IOThread* io_thread)
    : io_thread_(io_thread) {
  DCHECK(io_thread_);
}

NetworkQualityEstimatorProviderImpl::~NetworkQualityEstimatorProviderImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

scoped_refptr<base::SequencedTaskRunner>
NetworkQualityEstimatorProviderImpl::GetTaskRunner() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // |this| is constructed on UI thread, but must be used on the IO thread.
  thread_checker_.DetachFromThread();
  return content::BrowserThread::GetTaskRunnerForThread(
      content::BrowserThread::IO);
}

net::NetworkQualityEstimator*
NetworkQualityEstimatorProviderImpl::GetNetworkQualityEstimator() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  return io_thread_->globals()->network_quality_estimator.get();
}

}  // namespace metrics
