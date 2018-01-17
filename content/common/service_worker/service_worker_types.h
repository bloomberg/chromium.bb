// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/common/referrer.h"
#include "content/public/common/request_context_type.h"
#include "content/public/common/service_worker_modes.h"
#include "services/network/public/interfaces/fetch_api.mojom.h"
#include "services/network/public/interfaces/request_context_frame_type.mojom.h"
#include "third_party/WebKit/common/page/page_visibility_state.mojom.h"
#include "third_party/WebKit/common/service_worker/service_worker_client.mojom.h"
#include "third_party/WebKit/common/service_worker/service_worker_object.mojom.h"
#include "third_party/WebKit/common/service_worker/service_worker_registration.mojom.h"
#include "third_party/WebKit/common/service_worker/service_worker_state.mojom.h"
#include "third_party/WebKit/public/platform/modules/cache_storage/cache_storage.mojom.h"
#include "third_party/WebKit/public/platform/modules/fetch/fetch_api_request.mojom.h"
#include "url/gurl.h"

// This file is to have common definitions that are to be shared by
// browser and child process.

namespace storage {
class BlobHandle;
}

namespace content {

// Indicates the document main thread ID in the child process. This is used for
// messaging between the browser process and the child process.
static const int kDocumentMainThreadId = 0;

// Constants for error messages.
extern const char kServiceWorkerRegisterErrorPrefix[];
extern const char kServiceWorkerUpdateErrorPrefix[];
extern const char kServiceWorkerUnregisterErrorPrefix[];
extern const char kServiceWorkerGetRegistrationErrorPrefix[];
extern const char kServiceWorkerGetRegistrationsErrorPrefix[];
extern const char kFetchScriptError[];

// Constants for invalid identifiers.
static const int64_t kInvalidServiceWorkerResourceId = -1;
static const int kInvalidEmbeddedWorkerThreadId = -1;

// The HTTP cache is bypassed for Service Worker scripts if the last network
// fetch occurred over 24 hours ago.
static constexpr base::TimeDelta kServiceWorkerScriptMaxCacheAge =
    base::TimeDelta::FromHours(24);

struct ServiceWorkerCaseInsensitiveCompare {
  bool operator()(const std::string& lhs, const std::string& rhs) const {
    return base::CompareCaseInsensitiveASCII(lhs, rhs) < 0;
  }
};

using ServiceWorkerHeaderMap =
    std::map<std::string, std::string, ServiceWorkerCaseInsensitiveCompare>;

using ServiceWorkerHeaderList = std::vector<std::string>;

// To dispatch fetch request from browser to child process.
struct CONTENT_EXPORT ServiceWorkerFetchRequest {
  ServiceWorkerFetchRequest();
  ServiceWorkerFetchRequest(const GURL& url,
                            const std::string& method,
                            const ServiceWorkerHeaderMap& headers,
                            const Referrer& referrer,
                            bool is_reload);
  ServiceWorkerFetchRequest(const ServiceWorkerFetchRequest& other);
  ServiceWorkerFetchRequest& operator=(const ServiceWorkerFetchRequest& other);
  ~ServiceWorkerFetchRequest();
  size_t EstimatedStructSize();

  static blink::mojom::FetchCacheMode GetCacheModeFromLoadFlags(int load_flags);

  // Be sure to update EstimatedStructSize() when adding members.
  network::mojom::FetchRequestMode mode =
      network::mojom::FetchRequestMode::kNoCORS;
  bool is_main_resource_load = false;
  RequestContextType request_context_type = REQUEST_CONTEXT_TYPE_UNSPECIFIED;
  network::mojom::RequestContextFrameType frame_type =
      network::mojom::RequestContextFrameType::kNone;
  GURL url;
  std::string method;
  ServiceWorkerHeaderMap headers;
  std::string blob_uuid;
  uint64_t blob_size = 0;
  scoped_refptr<storage::BlobHandle> blob;
  Referrer referrer;
  network::mojom::FetchCredentialsMode credentials_mode =
      network::mojom::FetchCredentialsMode::kOmit;
  blink::mojom::FetchCacheMode cache_mode =
      blink::mojom::FetchCacheMode::kDefault;
  network::mojom::FetchRedirectMode redirect_mode =
      network::mojom::FetchRedirectMode::kFollow;
  std::string integrity;
  bool keepalive = false;
  std::string client_id;
  bool is_reload = false;
  ServiceWorkerFetchType fetch_type = ServiceWorkerFetchType::FETCH;
};

// Represents a response to a fetch.
struct CONTENT_EXPORT ServiceWorkerResponse {
  ServiceWorkerResponse();
  ServiceWorkerResponse(
      std::unique_ptr<std::vector<GURL>> url_list,
      int status_code,
      const std::string& status_text,
      network::mojom::FetchResponseType response_type,
      std::unique_ptr<ServiceWorkerHeaderMap> headers,
      const std::string& blob_uuid,
      uint64_t blob_size,
      scoped_refptr<storage::BlobHandle> blob,
      blink::mojom::ServiceWorkerResponseError error,
      base::Time response_time,
      bool is_in_cache_storage,
      const std::string& cache_storage_cache_name,
      std::unique_ptr<ServiceWorkerHeaderList> cors_exposed_header_names);
  ServiceWorkerResponse(const ServiceWorkerResponse& other);
  ServiceWorkerResponse& operator=(const ServiceWorkerResponse& other);
  ~ServiceWorkerResponse();
  size_t EstimatedStructSize();

  // Be sure to update EstimatedStructSize() when adding members.
  std::vector<GURL> url_list;
  int status_code;
  std::string status_text;
  network::mojom::FetchResponseType response_type;
  ServiceWorkerHeaderMap headers;
  // |blob_uuid| and |blob_size| are set when the body is a blob. For other
  // types of responses, the body is provided separately in Mojo IPC via
  // ServiceWorkerFetchResponseCallback.
  std::string blob_uuid;
  uint64_t blob_size;
  // |blob| is only used when features::kMojoBlobs is enabled.
  scoped_refptr<storage::BlobHandle> blob;
  blink::mojom::ServiceWorkerResponseError error;
  base::Time response_time;
  bool is_in_cache_storage = false;
  std::string cache_storage_cache_name;
  ServiceWorkerHeaderList cors_exposed_header_names;

  // Side data is used to pass the metadata of the response (eg: V8 code cache).
  std::string side_data_blob_uuid;
  uint64_t side_data_blob_size = 0;
  // |side_data_blob| is only used when features::kMojoBlobs is enabled.
  scoped_refptr<storage::BlobHandle> side_data_blob;
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
