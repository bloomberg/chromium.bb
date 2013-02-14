// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_NET_ERROR_TRACKER_H_
#define CHROME_COMMON_NET_NET_ERROR_TRACKER_H_

#include "base/bind.h"

class NetErrorTracker {
 public:
  enum FrameType {
    FRAME_SUB,
    FRAME_MAIN
  };

  enum PageType {
    PAGE_NORMAL,
    PAGE_ERROR
  };

  enum ErrorType {
    ERROR_OTHER,
    ERROR_DNS
  };

  enum DnsErrorPageState {
    DNS_ERROR_PAGE_NONE,
    DNS_ERROR_PAGE_PENDING,
    DNS_ERROR_PAGE_LOADED
  };

  typedef base::Callback<void(DnsErrorPageState state)> Callback;

  explicit NetErrorTracker(const Callback& callback);
  ~NetErrorTracker();

  void OnStartProvisionalLoad(FrameType frame, PageType page);
  void OnCommitProvisionalLoad(FrameType frame);
  void OnFailProvisionalLoad(FrameType frame, ErrorType error);
  void OnFinishLoad(FrameType frame);

 private:
  enum LoadState {
    LOAD_NONE,
    LOAD_STARTED,
    LOAD_COMMITTED,
    LOAD_FAILED,
    LOAD_FINISHED
  };

  void SetDnsErrorPageState(DnsErrorPageState state);

  Callback callback_;

  LoadState load_state_;
  PageType load_type_;

  ErrorType error_type_;

  DnsErrorPageState dns_error_page_state_;

  DISALLOW_COPY_AND_ASSIGN(NetErrorTracker);
};

#endif  // CHROME_COMMON_NET_NET_ERROR_TRACKER_H_
