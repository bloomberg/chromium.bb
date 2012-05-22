// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_URL_FETCHER_DELEGATE_H_
#define CONTENT_PUBLIC_COMMON_URL_FETCHER_DELEGATE_H_
#pragma once

// TODO(akalin): Remove this file once rlz is updated to use
// net::URLFetcherDelegate.

#ifndef RLZ_LIB_FINANCIAL_PING_H_
#error Only rlz/lib/financial_ping.h should include this file
#endif

#include "net/url_request/url_fetcher_delegate.h"

namespace content {

typedef net::URLFetcherDelegate URLFetcherDelegate;

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_URL_FETCHER_DELEGATE_H_
