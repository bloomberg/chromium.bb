// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_URL_FETCHER_DELEGATE_H_
#define CONTENT_PUBLIC_COMMON_URL_FETCHER_DELEGATE_H_
#pragma once

#include "net/url_request/url_fetcher_delegate.h"

namespace content {

// TODO(akalin): Update all users of URLFetcherDelegate to use
// net::URLFetcherDelegate and remove this file.

class URLFetcher;

// We inherit from net::URLFetcherDelegate so that we can still
// forward-declare URLFetcherDelegate (which we can't do with a
// typedef).
class URLFetcherDelegate : public net::URLFetcherDelegate {};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_URL_FETCHER_DELEGATE_H_
