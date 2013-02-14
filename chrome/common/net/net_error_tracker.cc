// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/net_error_tracker.h"

NetErrorTracker::NetErrorTracker(const Callback& callback)
    : callback_(callback),
      load_state_(LOAD_NONE),
      load_type_(PAGE_NORMAL),
      error_type_(ERROR_OTHER),
      dns_error_page_state_(DNS_ERROR_PAGE_NONE) {
}

NetErrorTracker::~NetErrorTracker() {
}

void NetErrorTracker::OnStartProvisionalLoad(FrameType frame, PageType page) {
  if (frame == FRAME_SUB)
    return;

  load_state_ = LOAD_STARTED;
  load_type_ = page;

  // TODO(ttuttle): Add support for aborts, then move this to OnCommit.
  if (load_type_ == PAGE_NORMAL)
    SetDnsErrorPageState(DNS_ERROR_PAGE_NONE);
}

void NetErrorTracker::OnCommitProvisionalLoad(FrameType frame) {
  if (frame == FRAME_SUB)
    return;

  load_state_ = LOAD_COMMITTED;
}

void NetErrorTracker::OnFailProvisionalLoad(FrameType frame, ErrorType error) {
  if (frame == FRAME_SUB)
    return;

  load_state_ = LOAD_FAILED;

  if (load_type_ == PAGE_NORMAL) {
    error_type_ = error;
    if (error_type_ == ERROR_DNS)
      SetDnsErrorPageState(DNS_ERROR_PAGE_PENDING);
  }
}

void NetErrorTracker::OnFinishLoad(FrameType frame) {
  if (frame == FRAME_SUB)
    return;

  load_state_ = LOAD_FINISHED;

  if (load_type_ == PAGE_ERROR && error_type_ == ERROR_DNS)
    SetDnsErrorPageState(DNS_ERROR_PAGE_LOADED);
}

void NetErrorTracker::SetDnsErrorPageState(DnsErrorPageState state) {
  if (state == dns_error_page_state_)
    return;

  dns_error_page_state_ = state;
  callback_.Run(state);
}
