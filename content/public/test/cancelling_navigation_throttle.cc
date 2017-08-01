// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/cancelling_navigation_throttle.h"

#include "base/bind.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"

namespace content {

CancellingNavigationThrottle::CancellingNavigationThrottle(
    NavigationHandle* handle,
    CancelTime cancel_time,
    ResultSynchrony sync)
    : NavigationThrottle(handle),
      cancel_time_(cancel_time),
      sync_(sync),
      weak_ptr_factory_(this) {}

CancellingNavigationThrottle::~CancellingNavigationThrottle() {}

NavigationThrottle::ThrottleCheckResult
CancellingNavigationThrottle::WillStartRequest() {
  return ProcessState(cancel_time_ == WILL_START_REQUEST);
}

NavigationThrottle::ThrottleCheckResult
CancellingNavigationThrottle::WillRedirectRequest() {
  return ProcessState(cancel_time_ == WILL_REDIRECT_REQUEST);
}

NavigationThrottle::ThrottleCheckResult
CancellingNavigationThrottle::WillProcessResponse() {
  return ProcessState(cancel_time_ == WILL_PROCESS_RESPONSE);
}

void CancellingNavigationThrottle::OnWillCancel() {}

NavigationThrottle::ThrottleCheckResult
CancellingNavigationThrottle::ProcessState(bool should_cancel) {
  if (sync_ == ASYNCHRONOUS) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&CancellingNavigationThrottle::MaybeCancel,
                   weak_ptr_factory_.GetWeakPtr(), should_cancel));
    return NavigationThrottle::DEFER;
  }
  if (should_cancel) {
    OnWillCancel();
    return NavigationThrottle::CANCEL;
  }
  return NavigationThrottle::PROCEED;
}

const char* CancellingNavigationThrottle::GetNameForLogging() {
  return "CancellingNavigationThrottle";
}

void CancellingNavigationThrottle::MaybeCancel(bool cancel) {
  if (cancel) {
    OnWillCancel();
    CancelDeferredNavigation(NavigationThrottle::CANCEL);
  } else {
    Resume();
  }
}

}  // namespace content
