// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_services_manager.h"

#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/metrics/metrics_state_manager.h"
#include "chrome/browser/metrics/variations/variations_service.h"
#include "components/rappor/rappor_service.h"

MetricsServicesManager::MetricsServicesManager(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state);
}

MetricsServicesManager::~MetricsServicesManager() {
}

MetricsService* MetricsServicesManager::GetMetricsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!metrics_service_)
    metrics_service_.reset(new MetricsService(GetMetricsStateManager()));
  return metrics_service_.get();
}

rappor::RapporService* MetricsServicesManager::GetRapporService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!rappor_service_)
    rappor_service_.reset(new rappor::RapporService);
  return rappor_service_.get();
}

chrome_variations::VariationsService*
MetricsServicesManager::GetVariationsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!variations_service_) {
    variations_service_ =
        chrome_variations::VariationsService::Create(local_state_,
                                                     GetMetricsStateManager());
  }
  return variations_service_.get();
}

metrics::MetricsStateManager* MetricsServicesManager::GetMetricsStateManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!metrics_state_manager_)
    metrics_state_manager_ = metrics::MetricsStateManager::Create(local_state_);
  return metrics_state_manager_.get();
}
