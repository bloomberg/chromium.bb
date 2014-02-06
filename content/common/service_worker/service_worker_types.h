// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

// This file is to have common definitions that are to be shared by
// browser and child process.

namespace content {

// Indicates invalid request ID (i.e. the sender does not expect it gets
// response for the message) for messaging between browser process
// and embedded worker.
const static int kInvalidRequestId = -1;

// To dispatch fetch request from browser to child process.
// TODO(kinuko): This struct will definitely need more fields and
// we'll probably want to have response struct/class too.
struct CONTENT_EXPORT ServiceWorkerFetchRequest {
  ServiceWorkerFetchRequest();
  ServiceWorkerFetchRequest(
      const GURL& url,
      const std::string& method,
      const std::map<std::string, std::string>& headers);
  ~ServiceWorkerFetchRequest();

  GURL url;
  std::string method;
  std::map<std::string, std::string> headers;
};

}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_
