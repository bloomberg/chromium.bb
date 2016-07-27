// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/renderer/document_subresource_filter.h"

#include <climits>

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "third_party/WebKit/public/platform/WebURL.h"

namespace subresource_filter {

namespace {

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

}  // namespace

DocumentSubresourceFilter::DocumentSubresourceFilter(
    ActivationState activation_state,
    const scoped_refptr<const MemoryMappedRuleset>& ruleset,
    std::vector<GURL> ancestor_document_urls)
    : activation_state_(activation_state),
      ruleset_(ruleset),
      matcher_(ruleset_->data(), ruleset_->length()),
      ancestor_document_urls_(std::move(ancestor_document_urls)) {
  DCHECK_NE(activation_state_, ActivationState::DISABLED);
  DCHECK(ruleset);
}

DocumentSubresourceFilter::~DocumentSubresourceFilter() = default;

bool DocumentSubresourceFilter::allowLoad(
    const blink::WebURL& resourceUrl,
    blink::WebURLRequest::RequestContext request_context) {
  TRACE_EVENT1("loader", "DocumentSubresourceFilter::allowLoad", "url",
               resourceUrl.string().utf8());

  // TODO(pkalinnikov): Check all |ancestor_document_urls| for activation bit
  // here or once in the constructor.
  ++num_loads_evaluated_;

  url::Origin initiator;
  if (!ancestor_document_urls_.empty())
    initiator = url::Origin(ancestor_document_urls_.front());

  if (!matcher_.IsAllowed(GURL(resourceUrl), initiator,
                          ToElementType(request_context))) {
    ++num_loads_matching_rules_;
    if (activation_state_ == ActivationState::ENABLED) {
      ++num_loads_disallowed_;
      return false;
    }
  }
  return true;
}

}  // namespace subresource_filter
