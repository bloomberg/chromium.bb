// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/renderer/web_document_subresource_filter_impl.h"

#include <utility>

#include "base/memory/ref_counted.h"
#include "components/subresource_filter/core/common/activation_state.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/common/proto/rules.pb.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_filter {

namespace {

using WebLoadPolicy = blink::WebDocumentSubresourceFilter::LoadPolicy;

proto::ElementType ToElementType(
    blink::WebURLRequest::RequestContext request_context) {
  switch (request_context) {
    case blink::WebURLRequest::RequestContextAudio:
    case blink::WebURLRequest::RequestContextVideo:
    case blink::WebURLRequest::RequestContextTrack:
      return proto::ELEMENT_TYPE_MEDIA;
    case blink::WebURLRequest::RequestContextBeacon:
    case blink::WebURLRequest::RequestContextPing:
      return proto::ELEMENT_TYPE_PING;
    case blink::WebURLRequest::RequestContextEmbed:
    case blink::WebURLRequest::RequestContextObject:
    case blink::WebURLRequest::RequestContextPlugin:
      return proto::ELEMENT_TYPE_OBJECT;
    case blink::WebURLRequest::RequestContextEventSource:
    case blink::WebURLRequest::RequestContextFetch:
    case blink::WebURLRequest::RequestContextXMLHttpRequest:
      return proto::ELEMENT_TYPE_XMLHTTPREQUEST;
    case blink::WebURLRequest::RequestContextFavicon:
    case blink::WebURLRequest::RequestContextImage:
    case blink::WebURLRequest::RequestContextImageSet:
      return proto::ELEMENT_TYPE_IMAGE;
    case blink::WebURLRequest::RequestContextFont:
      return proto::ELEMENT_TYPE_FONT;
    case blink::WebURLRequest::RequestContextFrame:
    case blink::WebURLRequest::RequestContextForm:
    case blink::WebURLRequest::RequestContextHyperlink:
    case blink::WebURLRequest::RequestContextIframe:
    case blink::WebURLRequest::RequestContextInternal:
    case blink::WebURLRequest::RequestContextLocation:
      return proto::ELEMENT_TYPE_SUBDOCUMENT;
    case blink::WebURLRequest::RequestContextScript:
    case blink::WebURLRequest::RequestContextServiceWorker:
    case blink::WebURLRequest::RequestContextSharedWorker:
      return proto::ELEMENT_TYPE_SCRIPT;
    case blink::WebURLRequest::RequestContextStyle:
    case blink::WebURLRequest::RequestContextXSLT:
      return proto::ELEMENT_TYPE_STYLESHEET;

    case blink::WebURLRequest::RequestContextPrefetch:
    case blink::WebURLRequest::RequestContextSubresource:
      return proto::ELEMENT_TYPE_OTHER;

    case blink::WebURLRequest::RequestContextCSPReport:
    case blink::WebURLRequest::RequestContextDownload:
    case blink::WebURLRequest::RequestContextImport:
    case blink::WebURLRequest::RequestContextManifest:
    case blink::WebURLRequest::RequestContextUnspecified:
    default:
      return proto::ELEMENT_TYPE_UNSPECIFIED;
  }
}

WebLoadPolicy ToWebLoadPolicy(LoadPolicy load_policy) {
  switch (load_policy) {
    case LoadPolicy::ALLOW:
      return WebLoadPolicy::Allow;
    case LoadPolicy::DISALLOW:
      return WebLoadPolicy::Disallow;
    case LoadPolicy::WOULD_DISALLOW:
      return WebLoadPolicy::WouldDisallow;
    default:
      NOTREACHED();
      return WebLoadPolicy::Allow;
  }
}

}  // namespace

WebDocumentSubresourceFilterImpl::~WebDocumentSubresourceFilterImpl() = default;

WebDocumentSubresourceFilterImpl::WebDocumentSubresourceFilterImpl(
    url::Origin document_origin,
    ActivationState activation_state,
    scoped_refptr<const MemoryMappedRuleset> ruleset,
    base::OnceClosure first_disallowed_load_callback)
    : filter_(std::move(document_origin), activation_state, std::move(ruleset)),
      first_disallowed_load_callback_(
          std::move(first_disallowed_load_callback)) {}

blink::WebDocumentSubresourceFilter::LoadPolicy
WebDocumentSubresourceFilterImpl::getLoadPolicy(
    const blink::WebURL& resourceUrl,
    blink::WebURLRequest::RequestContext request_context) {
  if (filter_.activation_state().filtering_disabled_for_document ||
      resourceUrl.protocolIs(url::kDataScheme)) {
    ++filter_.statistics().num_loads_total;
    return WebLoadPolicy::Allow;
  }

  // TODO(pkalinnikov): Would be good to avoid converting to GURL.
  return ToWebLoadPolicy(
      filter_.GetLoadPolicy(GURL(resourceUrl), ToElementType(request_context)));
}

void WebDocumentSubresourceFilterImpl::reportDisallowedLoad() {
  if (!first_disallowed_load_callback_.is_null())
    std::move(first_disallowed_load_callback_).Run();
}

}  // namespace subresource_filter
