// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_type_util.h"

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/service_worker/service_worker_types.h"
#include "third_party/WebKit/public/platform/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRequest.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerResponse.h"

namespace content {

namespace {

class HeaderVisitor : public blink::WebHTTPHeaderVisitor {
 public:
  explicit HeaderVisitor(ServiceWorkerHeaderMap* headers) : headers_(headers) {}
  ~HeaderVisitor() override {}

  void visitHeader(const blink::WebString& name,
                   const blink::WebString& value) override {
    // Headers are ISO Latin 1.
    const std::string& header_name = name.latin1();
    const std::string& header_value = value.latin1();
    CHECK(header_name.find('\0') == std::string::npos);
    CHECK(header_value.find('\0') == std::string::npos);
    headers_->insert(ServiceWorkerHeaderMap::value_type(
        header_name, header_value));
  }

 private:
  ServiceWorkerHeaderMap* const headers_;
};

std::unique_ptr<HeaderVisitor> MakeHeaderVisitor(
    ServiceWorkerHeaderMap* headers) {
  return std::unique_ptr<HeaderVisitor>(new HeaderVisitor(headers));
}

std::unique_ptr<ServiceWorkerHeaderMap> GetHeaderMap(
    const blink::WebServiceWorkerResponse& web_response) {
  std::unique_ptr<ServiceWorkerHeaderMap> result =
      base::MakeUnique<ServiceWorkerHeaderMap>();
  web_response.visitHTTPHeaderFields(MakeHeaderVisitor(result.get()).get());
  return result;
}

std::unique_ptr<ServiceWorkerHeaderList> GetHeaderList(
    const blink::WebVector<blink::WebString>& web_headers) {
  std::unique_ptr<ServiceWorkerHeaderList> result =
      base::MakeUnique<ServiceWorkerHeaderList>(web_headers.size());
  std::transform(web_headers.begin(), web_headers.end(), result->begin(),
                 [](const blink::WebString& s) { return s.latin1(); });
  return result;
}

std::unique_ptr<std::vector<GURL>> GetURLList(
    const blink::WebVector<blink::WebURL>& web_url_list) {
  std::unique_ptr<std::vector<GURL>> result =
      base::MakeUnique<std::vector<GURL>>(web_url_list.size());
  std::transform(web_url_list.begin(), web_url_list.end(), result->begin(),
                 [](const blink::WebURL& url) { return url; });
  return result;
}

}  // namespace

void GetServiceWorkerHeaderMapFromWebRequest(
    const blink::WebServiceWorkerRequest& web_request,
    ServiceWorkerHeaderMap* headers) {
  DCHECK(headers);
  DCHECK(headers->empty());
  web_request.visitHTTPHeaderFields(MakeHeaderVisitor(headers).get());
}

ServiceWorkerResponse GetServiceWorkerResponseFromWebResponse(
    const blink::WebServiceWorkerResponse& web_response) {
  return ServiceWorkerResponse(
      GetURLList(web_response.urlList()), web_response.status(),
      web_response.statusText().utf8(), web_response.responseType(),
      GetHeaderMap(web_response), web_response.blobUUID().utf8(),
      web_response.blobSize(), web_response.streamURL(), web_response.error(),
      base::Time::FromInternalValue(web_response.responseTime()),
      !web_response.cacheStorageCacheName().isNull(),
      web_response.cacheStorageCacheName().utf8(),
      GetHeaderList(web_response.corsExposedHeaderNames()));
}

}  // namespace content
