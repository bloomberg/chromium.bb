// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_RESOURCE_FETCH_CONTEXT_H_
#define MEDIA_BLINK_RESOURCE_FETCH_CONTEXT_H_

#include <memory>

#include "media/blink/media_blink_export.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebAssociatedURLLoader.h"
#include "third_party/WebKit/public/web/WebAssociatedURLLoaderOptions.h"

namespace media {

class MEDIA_BLINK_EXPORT ResourceFetchContext {
 public:
  virtual ~ResourceFetchContext() {}

  virtual std::unique_ptr<blink::WebAssociatedURLLoader> CreateUrlLoader(
      const blink::WebAssociatedURLLoaderOptions& options) = 0;
};

}  // namespace media

#endif  // MEDIA_BLINK_RESOURCE_FETCH_CONTEXT_H_
