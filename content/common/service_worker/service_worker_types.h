// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/strings/string_util.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerState.h"
#include "url/gurl.h"

// This file is to have common definitions that are to be shared by
// browser and child process.

namespace content {

// Indicates invalid request ID (i.e. the sender does not expect it gets
// response for the message) for messaging between browser process
// and embedded worker.
static const int kInvalidServiceWorkerRequestId = -1;

// Constants for invalid identifiers.
static const int kInvalidServiceWorkerHandleId = -1;
static const int kInvalidServiceWorkerRegistrationHandleId = -1;
static const int kInvalidServiceWorkerProviderId = -1;
static const int64 kInvalidServiceWorkerRegistrationId = -1;
static const int64 kInvalidServiceWorkerVersionId = -1;
static const int64 kInvalidServiceWorkerResourceId = -1;
static const int64 kInvalidServiceWorkerResponseId = -1;
static const int kInvalidEmbeddedWorkerThreadId = -1;

// Indicates how the service worker handled a fetch event.
enum ServiceWorkerFetchEventResult {
  // Browser should fallback to native fetch.
  SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK,
  // Service worker provided a ServiceWorkerResponse.
  SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE,
  SERVICE_WORKER_FETCH_EVENT_LAST = SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE
};

struct ServiceWorkerCaseInsensitiveCompare {
  bool operator()(const std::string& lhs, const std::string& rhs) const {
    return base::strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
  }
};

typedef std::map<std::string, std::string, ServiceWorkerCaseInsensitiveCompare>
    ServiceWorkerHeaderMap;

// To dispatch fetch request from browser to child process.
struct CONTENT_EXPORT ServiceWorkerFetchRequest {
  ServiceWorkerFetchRequest();
  ServiceWorkerFetchRequest(const GURL& url,
                            const std::string& method,
                            const ServiceWorkerHeaderMap& headers,
                            const GURL& referrer,
                            bool is_reload);
  ~ServiceWorkerFetchRequest();

  GURL url;
  std::string method;
  ServiceWorkerHeaderMap headers;
  std::string blob_uuid;
  uint64 blob_size;
  GURL referrer;
  bool is_reload;
};

// Represents a response to a fetch.
struct CONTENT_EXPORT ServiceWorkerResponse {
  ServiceWorkerResponse();
  ServiceWorkerResponse(const GURL& url,
                        int status_code,
                        const std::string& status_text,
                        const ServiceWorkerHeaderMap& headers,
                        const std::string& blob_uuid);
  ~ServiceWorkerResponse();

  GURL url;
  int status_code;
  std::string status_text;
  ServiceWorkerHeaderMap headers;
  std::string blob_uuid;
};

// Controls how requests are matched in the Cache API.
struct CONTENT_EXPORT ServiceWorkerCacheQueryParams {
  ServiceWorkerCacheQueryParams();

  bool ignore_search;
  bool ignore_method;
  bool ignore_vary;
  bool prefix_match;
};

// The type of a single batch operation in the Cache API.
enum ServiceWorkerCacheOperationType {
  SERVICE_WORKER_CACHE_OPERATION_TYPE_UNDEFINED,
  SERVICE_WORKER_CACHE_OPERATION_TYPE_PUT,
  SERVICE_WORKER_CACHE_OPERATION_TYPE_DELETE,
  SERVICE_WORKER_CACHE_OPERATION_TYPE_LAST =
      SERVICE_WORKER_CACHE_OPERATION_TYPE_DELETE
};

// A single batch operation for the Cache API.
struct CONTENT_EXPORT ServiceWorkerBatchOperation {
  ServiceWorkerBatchOperation();

  ServiceWorkerCacheOperationType operation_type;
  ServiceWorkerFetchRequest request;
  ServiceWorkerResponse response;
  ServiceWorkerCacheQueryParams match_params;
};

// Represents initialization info for a WebServiceWorker object.
struct CONTENT_EXPORT ServiceWorkerObjectInfo {
  ServiceWorkerObjectInfo();
  int handle_id;
  GURL scope;
  GURL url;
  blink::WebServiceWorkerState state;
};

struct ServiceWorkerRegistrationObjectInfo {
  ServiceWorkerRegistrationObjectInfo();
  int handle_id;
  GURL scope;
};

struct ServiceWorkerVersionAttributes {
  ServiceWorkerObjectInfo installing;
  ServiceWorkerObjectInfo waiting;
  ServiceWorkerObjectInfo active;
};

class ChangedVersionAttributesMask {
 public:
  enum {
    INSTALLING_VERSION = 1 << 0,
    WAITING_VERSION = 1 << 1,
    ACTIVE_VERSION = 1 << 2,
    CONTROLLING_VERSION = 1 << 3,
  };

  ChangedVersionAttributesMask() : changed_(0) {}
  explicit ChangedVersionAttributesMask(int changed) : changed_(changed) {}

  int changed() const { return changed_; }

  void add(int changed_versions) { changed_ |= changed_versions; }
  bool installing_changed() const { return !!(changed_ & INSTALLING_VERSION); }
  bool waiting_changed() const { return !!(changed_ & WAITING_VERSION); }
  bool active_changed() const { return !!(changed_ & ACTIVE_VERSION); }
  bool controller_changed() const { return !!(changed_ & CONTROLLING_VERSION); }

 private:
  int changed_;
};

}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_
