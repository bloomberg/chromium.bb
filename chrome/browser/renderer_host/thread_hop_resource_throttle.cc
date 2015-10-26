// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/thread_hop_resource_throttle.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_controller.h"

ThreadHopResourceThrottle::ThreadHopResourceThrottle() : weak_factory_(this) {}

ThreadHopResourceThrottle::~ThreadHopResourceThrottle() {}

bool ThreadHopResourceThrottle::IsEnabled() {
  const std::string group_name =
      base::FieldTrialList::FindFullName("ThreadHopResourceThrottle");
  return base::StartsWith(group_name, "Enabled", base::CompareCase::SENSITIVE);
}

void ThreadHopResourceThrottle::WillStartRequest(bool* defer) {
  *defer = true;
  ResumeAfterThreadHop();
}

void ThreadHopResourceThrottle::WillRedirectRequest(
    const net::RedirectInfo& redirect_info,
    bool* defer) {
  *defer = true;
  ResumeAfterThreadHop();
}

void ThreadHopResourceThrottle::WillProcessResponse(bool* defer) {
  *defer = true;
  ResumeAfterThreadHop();
}

const char* ThreadHopResourceThrottle::GetNameForLogging() const {
  return "ThreadHopResourceThrottle";
}

void ThreadHopResourceThrottle::ResumeAfterThreadHop() {
  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::UI, FROM_HERE, base::Bind(&base::DoNothing),
      base::Bind(&ThreadHopResourceThrottle::Resume, weak_factory_.GetWeakPtr(),
                 base::TimeTicks::Now()));
}

void ThreadHopResourceThrottle::Resume(const base::TimeTicks& time) {
  UMA_HISTOGRAM_TIMES("Net.ThreadHopResourceThrottleTime",
                      base::TimeTicks::Now() - time);
  controller()->Resume();
}
