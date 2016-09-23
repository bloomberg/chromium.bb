// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nqe/ui_network_quality_estimator_service.h"

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "content/public/browser/browser_thread.h"

// A class that sets itself as an observer of the EffectiveconnectionType for
// the browser IO thread. It reports any change in EffectiveConnectionType back
// to the UI service.
// It is created on the UI thread, but used and deleted on the IO thread.
class UINetworkQualityEstimatorService::IONetworkQualityObserver
    : public net::NetworkQualityEstimator::EffectiveConnectionTypeObserver {
 public:
  IONetworkQualityObserver(
      base::WeakPtr<UINetworkQualityEstimatorService> service)
      : service_(service) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  ~IONetworkQualityObserver() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    if (network_quality_estimator_)
      network_quality_estimator_->RemoveEffectiveConnectionTypeObserver(this);
  }

  void InitializeOnIOThread(IOThread* io_thread) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    if (!io_thread->globals()->network_quality_estimator)
      return;
    network_quality_estimator_ =
        io_thread->globals()->network_quality_estimator.get();
    if (!network_quality_estimator_)
      return;
    network_quality_estimator_->AddEffectiveConnectionTypeObserver(this);
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(
            &UINetworkQualityEstimatorService::EffectiveConnectionTypeChanged,
            service_,
            network_quality_estimator_->GetEffectiveConnectionType()));
  }

  // net::NetworkQualityEstimator::EffectiveConnectionTypeObserver
  // implementation:
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType type) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(
            &UINetworkQualityEstimatorService::EffectiveConnectionTypeChanged,
            service_, type));
  }

 private:
  base::WeakPtr<UINetworkQualityEstimatorService> service_;
  net::NetworkQualityEstimator* network_quality_estimator_;

  DISALLOW_COPY_AND_ASSIGN(IONetworkQualityObserver);
};

UINetworkQualityEstimatorService::UINetworkQualityEstimatorService()
    : type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
      io_observer_(nullptr),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // If this is running in a context without an IOThread, don't try to create
  // the IO object.
  if (!g_browser_process->io_thread())
    return;
  io_observer_ = new IONetworkQualityObserver(weak_factory_.GetWeakPtr());
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&IONetworkQualityObserver::InitializeOnIOThread,
                 base::Unretained(io_observer_),
                 g_browser_process->io_thread()));
}

UINetworkQualityEstimatorService::~UINetworkQualityEstimatorService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void UINetworkQualityEstimatorService::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  weak_factory_.InvalidateWeakPtrs();
  DCHECK(content::BrowserThread::DeleteSoon(content::BrowserThread::IO,
                                            FROM_HERE, io_observer_));
}

void UINetworkQualityEstimatorService::EffectiveConnectionTypeChanged(
    net::EffectiveConnectionType type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  type_ = type;
}

void UINetworkQualityEstimatorService::SetEffectiveConnectionTypeForTesting(
    net::EffectiveConnectionType type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  type_ = type;
}

net::EffectiveConnectionType
UINetworkQualityEstimatorService::GetEffectiveConnectionType() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return type_;
}
