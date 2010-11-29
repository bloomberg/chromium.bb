// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CEEE module-wide metrics utilities.

#ifndef CEEE_IE_COMMON_METRICS_UTIL_H__
#define CEEE_IE_COMMON_METRICS_UTIL_H__

#include <atlsafe.h>
#include <string>

#include "base/logging.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "base/win/windows_version.h"
#include "ceee/ie/broker/broker_rpc_client.h"

namespace metrics_util {

// This is a class that measures the time taken from its construction to its
// destruction. A good way to use it is to instantiate it on the stack.
// TODO(hansl): Make sure the BrokerRpcClient calls from this class are non-
//              blocking.
class ScopedTimer {
 public:
  // Callers must ensure broker_rpc exceeds the life of this object.
  ScopedTimer(const std::string name, BrokerRpcClient* broker_rpc)
      : name_(name), broker_rpc_(broker_rpc), start_(base::TimeTicks::Now()) {
    if (name.length() == 0) {
      NOTREACHED() << "Histogram name shouldn't be empty.";
      broker_rpc_ = NULL;  // Ensure we don't call the broker_rpc.
    }
  }

  ~ScopedTimer() {
    if (broker_rpc_) {
      base::TimeDelta delta = base::TimeTicks::Now() - start_;
      if (FAILED(broker_rpc_->SendUmaHistogramTimes(
              name_.c_str(), static_cast<int>(delta.InMilliseconds())))) {
        NOTREACHED() << "An error happened during RPC.";
      }
    }
  }

  void Drop() {
    // Something happened and we should not log.
    broker_rpc_ = NULL;
  }

 protected:
  base::TimeTicks start_;

  // The RPC Client to use when sending events.
  BrokerRpcClient* broker_rpc_;

  // The name of the event when sending it.
  std::string name_;
};

}

#endif  // CEEE_IE_COMMON_METRICS_UTIL_H__
