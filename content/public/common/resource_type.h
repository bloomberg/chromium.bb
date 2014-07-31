// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_RESOURCE_TYPE_H_
#define CONTENT_PUBLIC_COMMON_RESOURCE_TYPE_H_

#include "content/common/content_export.h"

namespace content {

// Used in histograms, so please add new types at the end, and rename unused
// entries to RESOURCETYPE_UNUSED_0, etc...
enum ResourceType {
  RESOURCE_TYPE_MAIN_FRAME = 0,  // top level page
  RESOURCE_TYPE_SUB_FRAME,       // frame or iframe
  RESOURCE_TYPE_STYLESHEET,      // a CSS stylesheet
  RESOURCE_TYPE_SCRIPT,          // an external script
  RESOURCE_TYPE_IMAGE,           // an image (jpg/gif/png/etc)
  RESOURCE_TYPE_FONT_RESOURCE,   // a font
  RESOURCE_TYPE_SUB_RESOURCE,    // an "other" subresource.
  RESOURCE_TYPE_OBJECT,          // an object (or embed) tag for a plugin,
                                 // or a resource that a plugin requested.
  RESOURCE_TYPE_MEDIA,           // a media resource.
  RESOURCE_TYPE_WORKER,          // the main resource of a dedicated worker.
  RESOURCE_TYPE_SHARED_WORKER,   // the main resource of a shared worker.
  RESOURCE_TYPE_PREFETCH,        // an explicitly requested prefetch
  RESOURCE_TYPE_FAVICON,         // a favicon
  RESOURCE_TYPE_XHR,             // a XMLHttpRequest
  RESOURCE_TYPE_PING,            // a ping request for <a ping>
  RESOURCE_TYPE_SERVICE_WORKER,  // the main resource of a service worker.
  RESOURCE_TYPE_LAST_TYPE
};

CONTENT_EXPORT bool IsResourceTypeFrame(ResourceType type);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_RESOURCE_TYPE_H_
