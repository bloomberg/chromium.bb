// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/process_resource_usage.h"

#include "base/bind.h"
#include "base/logging.h"

ProcessResourceUsage::ProcessResourceUsage(ResourceUsageReporterPtr service)
    : service_(service.Pass()), update_in_progress_(false) {
}

ProcessResourceUsage::~ProcessResourceUsage() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ProcessResourceUsage::Refresh() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!update_in_progress_ && service_) {
    update_in_progress_ = true;
    service_->GetUsageData(base::Bind(&ProcessResourceUsage::OnRefreshDone,
                                      base::Unretained(this)));
  }
}

void ProcessResourceUsage::OnRefreshDone(ResourceUsageDataPtr data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  update_in_progress_ = false;
  stats_ = data.Pass();
}

bool ProcessResourceUsage::ReportsV8MemoryStats() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (stats_)
    return stats_->reports_v8_stats;
  return false;
}

size_t ProcessResourceUsage::GetV8MemoryAllocated() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (stats_ && stats_->reports_v8_stats)
    return stats_->v8_bytes_allocated;
  return 0;
}

size_t ProcessResourceUsage::GetV8MemoryUsed() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (stats_ && stats_->reports_v8_stats)
    return stats_->v8_bytes_used;
  return 0;
}
