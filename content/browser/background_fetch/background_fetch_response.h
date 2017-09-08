// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_RESPONSE_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_RESPONSE_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

namespace content {

// Contains the response after a background fetch has started.
struct CONTENT_EXPORT BackgroundFetchResponse {
  BackgroundFetchResponse(
      const std::vector<GURL>& url_chain,
      const scoped_refptr<const net::HttpResponseHeaders>& headers);

  ~BackgroundFetchResponse();

  const std::vector<GURL> url_chain;
  const scoped_refptr<const net::HttpResponseHeaders> headers;  // May be null.

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchResponse);
};

struct CONTENT_EXPORT BackgroundFetchResult {
  BackgroundFetchResult(base::Time response_time,
                        const base::FilePath& path,
                        uint64_t file_size);

  ~BackgroundFetchResult();

  const base::Time response_time;
  const base::FilePath file_path;
  const uint64_t file_size;

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchResult);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_RESPONSE_H_
