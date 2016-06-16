// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_RESOURCE_REQUEST_BODY_H_
#define CONTENT_PUBLIC_COMMON_RESOURCE_REQUEST_BODY_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace content {

// ResourceRequestBody represents body (i.e. upload data) of a HTTP request.
//
// This class is intentionally opaque:
// - Embedders cannot inspect the payload of ResourceRequestBody.  Only the
//   //content layer can decompose ResourceRequestBody into references to file
//   ranges, byte vectors, blob uris, etc.
// - Embedders can get instances of ResourceRequestBody only by receiving an
//   instance created inside //content layer (e.g. receiving it via
//   content::OpenURLParams).
// - Embedders typically end up passing ResourceRequestBody back into the
//   //content layer via content::NavigationController::LoadUrlParams.
class CONTENT_EXPORT ResourceRequestBody
    : public base::RefCountedThreadSafe<ResourceRequestBody> {
 protected:
  ResourceRequestBody();
  virtual ~ResourceRequestBody();

 private:
  friend class base::RefCountedThreadSafe<ResourceRequestBody>;
  DISALLOW_COPY_AND_ASSIGN(ResourceRequestBody);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_RESOURCE_REQUEST_BODY_H_
