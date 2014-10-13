// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/service/cast_service.h"

#include "base/logging.h"
#include "base/threading/thread_checker.h"

namespace chromecast {

CastService::CastService(
    content::BrowserContext* browser_context,
    const OptInStatsChangedCallback& opt_in_stats_callback)
    : browser_context_(browser_context),
      opt_in_stats_callback_(opt_in_stats_callback),
      stopped_(true),
      thread_checker_(new base::ThreadChecker()) {
}

CastService::~CastService() {
  DCHECK(thread_checker_->CalledOnValidThread());
  DCHECK(stopped_);
}

void CastService::Start() {
  DCHECK(thread_checker_->CalledOnValidThread());

  Initialize();
  stopped_ = false;
  StartInternal();
}

void CastService::Stop() {
  DCHECK(thread_checker_->CalledOnValidThread());
  StopInternal();
  stopped_ = true;
}

}  // namespace chromecast
