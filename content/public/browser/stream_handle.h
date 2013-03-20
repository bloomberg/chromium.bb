// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_STREAM_HANDLE_H_
#define CONTENT_PUBLIC_BROWSER_STREAM_HANDLE_H_

#include "base/callback.h"
#include "content/common/content_export.h"
#include "googleurl/src/gurl.h"

namespace content {

class CONTENT_EXPORT StreamHandle {
 public:
  virtual ~StreamHandle() {}

  // Gets the URL the stream can be read from.
  virtual const GURL& GetURL() = 0;

  // Gets the original URL being redirected to this Stream.
  virtual const GURL& GetOriginalURL() = 0;

  // Get the MIME type associated with this Stream.
  virtual const std::string& GetMimeType() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_STREAM_HANDLE_H_
