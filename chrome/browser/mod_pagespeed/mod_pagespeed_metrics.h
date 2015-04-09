// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MOD_PAGESPEED_MOD_PAGESPEED_METRICS_H_
#define CHROME_BROWSER_MOD_PAGESPEED_MOD_PAGESPEED_METRICS_H_

#include "content/public/common/resource_type.h"

class GURL;

namespace net {
class HttpResponseHeaders;
}

namespace mod_pagespeed {

// Records metrics about mod_pagespeed headers.
//
// See
// https://developers.google.com/speed/pagespeed/module/configuration#XHeaderValue
void RecordMetrics(const content::ResourceType resource_type,
                   const GURL& request_url,
                   const net::HttpResponseHeaders* response_headers);

}  // namespace mod_pagespeed

#endif  // CHROME_BROWSER_MOD_PAGESPEED_MOD_PAGESPEED_METRICS_H_
