// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A Preconnect instance maintains state while a TCP/IP connection is made, and
// and then released into the pool of available connections for future use.

#ifndef CHROME_BROWSER_NET_PRECONNECT_H_
#define CHROME_BROWSER_NET_PRECONNECT_H_
#pragma once

#include "chrome/browser/net/url_info.h"

class GURL;

namespace chrome_browser_net {

// Try to preconnect.  Typically motivated by OMNIBOX to reach search service.
// |count| may be used to request more than one connection be established in
// parallel.
void PreconnectOnUIThread(const GURL& url,
                          UrlInfo::ResolutionMotivation motivation,
                          int count);

// Try to preconnect.  Typically used by predictor when a subresource probably
// needs a connection. |count| may be used to request more than one connection
// be established in parallel.
void PreconnectOnIOThread(const GURL& url,
                          UrlInfo::ResolutionMotivation motivation,
                          int count);

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_PRECONNECT_H_
