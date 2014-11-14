// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SDCH_NET_LOG_PARAMS_H_
#define NET_BASE_SDCH_NET_LOG_PARAMS_H_

#include <string>

#include "net/base/net_export.h"
#include "net/base/net_log.h"
#include "net/base/sdch_problem_codes.h"

class GURL;

namespace net {

NET_EXPORT base::Value* NetLogSdchResourceProblemCallback(
    SdchProblemCode problem,
    NetLog::LogLevel log_level);

// If |is_error| is false, "net_error" field won't be added to the JSON and the
// event won't be painted red in the netlog.
NET_EXPORT base::Value* NetLogSdchDictionaryFetchProblemCallback(
    SdchProblemCode problem,
    const GURL& url,
    bool is_error,
    NetLog::LogLevel log_level);

}  // namespace net

#endif  // NET_BASE_SDCH_NET_LOG_PARAMS_H_
