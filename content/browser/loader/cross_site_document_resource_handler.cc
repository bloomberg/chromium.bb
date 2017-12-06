// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/cross_site_document_resource_handler.h"

#include <string.h>
#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/loader/detachable_resource_handler.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/site_isolation_policy.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "content/public/common/content_client.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_sniffer.h"
#include "net/url_request/url_request.h"

namespace content {

namespace {

void LogCrossSiteDocumentAction(
    CrossSiteDocumentResourceHandler::Action action) {
  UMA_HISTOGRAM_ENUMERATION("SiteIsolation.XSD.Browser.Action", action,
                            CrossSiteDocumentResourceHandler::Action::kCount);
}

}  // namespace

// ResourceController used in OnWillRead in cases that sniffing will happen.
// When invoked, it runs the corresponding method on the ResourceHandler.
class CrossSiteDocumentResourceHandler::OnWillReadController
    : public ResourceController {
 public:
  // Keeps track of the addresses of the ResourceLoader's buffer and size,
  // which will be populated by the downstream ResourceHandler by the time that
  // Resume() is called.
  explicit OnWillReadController(
      CrossSiteDocumentResourceHandler* document_handler,
      scoped_refptr<net::IOBuffer>* buf,
      int* buf_size)
      : document_handler_(document_handler), buf_(buf), buf_size_(buf_size) {}

  ~OnWillReadController() override {}

  // ResourceController implementation:
  void Resume() override {
    MarkAsUsed();

    // Now that |buf_| has a buffer written into it by the downstream handler,
    // set up sniffing in the CrossSiteDocumentResourceHandler.
    document_handler_->ResumeOnWillRead(buf_, buf_size_);
  }

  void Cancel() override {
    MarkAsUsed();
    document_handler_->Cancel();
  }

  void CancelWithError(int error_code) override {
    MarkAsUsed();
    document_handler_->CancelWithError(error_code);
  }

 private:
  void MarkAsUsed() {
#if DCHECK_IS_ON()
    DCHECK(!used_);
    used_ = true;
#endif
  }

#if DCHECK_IS_ON()
  bool used_ = false;
#endif

  CrossSiteDocumentResourceHandler* document_handler_;

  // Address of the ResourceLoader's buffer, which will be populated by the
  // downstream handler before Resume() is called.
  scoped_refptr<net::IOBuffer>* buf_;

  // Address of the size of |buf_|, similarly populated downstream.
  int* buf_size_;

  DISALLOW_COPY_AND_ASSIGN(OnWillReadController);
};

CrossSiteDocumentResourceHandler::CrossSiteDocumentResourceHandler(
    std::unique_ptr<ResourceHandler> next_handler,
    net::URLRequest* request)
    : LayeredResourceHandler(request, std::move(next_handler)) {}

CrossSiteDocumentResourceHandler::~CrossSiteDocumentResourceHandler() {}

void CrossSiteDocumentResourceHandler::OnResponseStarted(
    ResourceResponse* response,
    std::unique_ptr<ResourceController> controller) {
  has_response_started_ = true;
  LogCrossSiteDocumentAction(
      CrossSiteDocumentResourceHandler::Action::kResponseStarted);

  should_block_based_on_headers_ = ShouldBlockBasedOnHeaders(response);
  next_handler_->OnResponseStarted(response, std::move(controller));
}

void CrossSiteDocumentResourceHandler::OnWillRead(
    scoped_refptr<net::IOBuffer>* buf,
    int* buf_size,
    std::unique_ptr<ResourceController> controller) {
  DCHECK(has_response_started_);

  // On the next read attempt after the response was blocked, either cancel the
  // rest of the request or allow it to proceed in a detached state.
  if (blocked_read_completed_) {
    DCHECK(should_block_based_on_headers_);
    DCHECK(!allow_based_on_sniffing_);
    const ResourceRequestInfoImpl* info = GetRequestInfo();
    if (info && info->detachable_handler()) {
      // Ensure that prefetch, etc, continue to cache the response, without
      // sending it to the renderer.
      info->detachable_handler()->Detach();
    } else {
      // If it's not detachable, cancel the rest of the request.
      controller->Cancel();
    }
    return;
  }

  // If we intended to block the response and haven't yet decided to allow it
  // due to sniffing, we will read some of the data to a local buffer to sniff
  // it.  Since the downstream handler may defer during the OnWillRead call
  // below, the values of |buf| and |buf_size| may not be available right away.
  // Instead, create an OnWillReadController to start the sniffing after the
  // downstream handler has called Resume on it.
  if (should_block_based_on_headers_ && !allow_based_on_sniffing_) {
    HoldController(std::move(controller));
    controller = std::make_unique<OnWillReadController>(this, buf, buf_size);
  }

  // Have the downstream handler(s) allocate the real buffer to use.
  next_handler_->OnWillRead(buf, buf_size, std::move(controller));
}

void CrossSiteDocumentResourceHandler::ResumeOnWillRead(
    scoped_refptr<net::IOBuffer>* buf,
    int* buf_size) {
  // We should only get here in cases that we intend to sniff the data, after
  // downstream handler finishes its work from OnWillRead.
  DCHECK(should_block_based_on_headers_);
  DCHECK(!allow_based_on_sniffing_);
  DCHECK(!blocked_read_completed_);

  // For most blocked responses, we need to sniff the data to confirm it looks
  // like the claimed MIME type (to avoid blocking mislabeled JavaScript,
  // JSONP, etc).  Read this data into a separate buffer (not shared with the
  // renderer), which we will only copy over if we decide to allow it through.
  // This is only done when we suspect the response should be blocked.
  //
  // Make it as big as the downstream handler's buffer to make it easy to copy
  // over in one operation.  This will be large, since the MIME sniffing
  // handler is downstream.  Technically we could use a smaller buffer if
  // |needs_sniffing_| is false, but there's no need for the extra complexity.
  DCHECK_GE(*buf_size, net::kMaxBytesToSniff);
  local_buffer_ =
      base::MakeRefCounted<net::IOBuffer>(static_cast<size_t>(*buf_size));

  // Store the next handler's buffer but don't read into it while sniffing,
  // since we probably won't want to send the data to the renderer process.
  next_handler_buffer_ = *buf;
  next_handler_buffer_size_ = *buf_size;
  *buf = local_buffer_;

  Resume();
}

void CrossSiteDocumentResourceHandler::OnReadCompleted(
    int bytes_read,
    std::unique_ptr<ResourceController> controller) {
  DCHECK(has_response_started_);
  DCHECK(!blocked_read_completed_);

  // If we intended to block the response and haven't sniffed yet, try to
  // confirm that we should block it.  If sniffing is needed, look at the local
  // buffer and either report that zero bytes were read (to indicate the
  // response is empty and complete), or copy the sniffed data to the next
  // handler's buffer and resume the response without blocking.
  if (should_block_based_on_headers_ && !allow_based_on_sniffing_) {
    bool confirmed_blockable = false;
    if (!needs_sniffing_) {
      // TODO(creis): Also consider the MIME type confirmed if |bytes_read| is
      // too small to do sniffing, or restructure to allow buffering enough.
      // For now, responses with small initial reads may be allowed through.
      confirmed_blockable = true;
    } else {
      // Sniff the data to see if it likely matches the MIME type that caused us
      // to decide to block it.  If it doesn't match, it may be JavaScript,
      // JSONP, or another allowable data type and we should let it through.
      // Record how many bytes were read to see how often it's too small.  (This
      // will typically be under 100,000.)
      UMA_HISTOGRAM_COUNTS("SiteIsolation.XSD.Browser.BytesReadForSniffing",
                           bytes_read);
      DCHECK_LE(bytes_read, next_handler_buffer_size_);
      base::StringPiece data(local_buffer_->data(), bytes_read);

      // Confirm whether the data is HTML, XML, or JSON.
      if (canonical_mime_type_ == CROSS_SITE_DOCUMENT_MIME_TYPE_HTML) {
        confirmed_blockable = CrossSiteDocumentClassifier::SniffForHTML(data);
      } else if (canonical_mime_type_ == CROSS_SITE_DOCUMENT_MIME_TYPE_XML) {
        confirmed_blockable = CrossSiteDocumentClassifier::SniffForXML(data);
      } else if (canonical_mime_type_ == CROSS_SITE_DOCUMENT_MIME_TYPE_JSON) {
        confirmed_blockable = CrossSiteDocumentClassifier::SniffForJSON(data);
      } else if (canonical_mime_type_ == CROSS_SITE_DOCUMENT_MIME_TYPE_PLAIN) {
        // For responses labeled as plain text, only block them if the data
        // sniffs as one of the formats we would block in the first place.
        confirmed_blockable = CrossSiteDocumentClassifier::SniffForHTML(data) ||
                              CrossSiteDocumentClassifier::SniffForXML(data) ||
                              CrossSiteDocumentClassifier::SniffForJSON(data);
      }
    }

    if (confirmed_blockable) {
      // Block the response and throw away the data.  Report zero bytes read.
      bytes_read = 0;
      blocked_read_completed_ = true;

      // Log the blocking event.  Inline the Serialize call to avoid it when
      // tracing is disabled.
      TRACE_EVENT2("navigation",
                   "CrossSiteDocumentResourceHandler::ShouldBlockResponse",
                   "initiator",
                   request()->initiator().has_value()
                       ? request()->initiator().value().Serialize()
                       : "null",
                   "url", request()->url().spec());

      LogCrossSiteDocumentAction(
          needs_sniffing_
              ? CrossSiteDocumentResourceHandler::Action::kBlockedAfterSniffing
              : CrossSiteDocumentResourceHandler::Action::
                    kBlockedWithoutSniffing);
      ResourceType resource_type = GetRequestInfo()->GetResourceType();
      UMA_HISTOGRAM_ENUMERATION("SiteIsolation.XSD.Browser.Blocked",
                                resource_type,
                                content::RESOURCE_TYPE_LAST_TYPE);
      switch (canonical_mime_type_) {
        case CROSS_SITE_DOCUMENT_MIME_TYPE_HTML:
          UMA_HISTOGRAM_ENUMERATION("SiteIsolation.XSD.Browser.Blocked.HTML",
                                    resource_type,
                                    content::RESOURCE_TYPE_LAST_TYPE);
          break;
        case CROSS_SITE_DOCUMENT_MIME_TYPE_XML:
          UMA_HISTOGRAM_ENUMERATION("SiteIsolation.XSD.Browser.Blocked.XML",
                                    resource_type,
                                    content::RESOURCE_TYPE_LAST_TYPE);
          break;
        case CROSS_SITE_DOCUMENT_MIME_TYPE_JSON:
          UMA_HISTOGRAM_ENUMERATION("SiteIsolation.XSD.Browser.Blocked.JSON",
                                    resource_type,
                                    content::RESOURCE_TYPE_LAST_TYPE);
          break;
        case CROSS_SITE_DOCUMENT_MIME_TYPE_PLAIN:
          UMA_HISTOGRAM_ENUMERATION("SiteIsolation.XSD.Browser.Blocked.Plain",
                                    resource_type,
                                    content::RESOURCE_TYPE_LAST_TYPE);
          break;
        default:
          NOTREACHED();
      }
    } else {
      // Allow the response through instead and proceed with reading more.
      // Copy sniffed data into the next handler's buffer before proceeding.
      // Note that the size of the two buffers is the same (see OnWillRead).
      DCHECK_LE(bytes_read, next_handler_buffer_size_);
      memcpy(next_handler_buffer_->data(), local_buffer_->data(), bytes_read);
      allow_based_on_sniffing_ = true;
    }

    // Clean up, whether we'll cancel or proceed from here.
    local_buffer_ = nullptr;
    next_handler_buffer_ = nullptr;
    next_handler_buffer_size_ = 0;
  }

  next_handler_->OnReadCompleted(bytes_read, std::move(controller));
}

void CrossSiteDocumentResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    std::unique_ptr<ResourceController> controller) {
  if (blocked_read_completed_) {
    // Report blocked responses as successful, rather than the cancellation
    // from OnWillRead.
    next_handler_->OnResponseCompleted(net::URLRequestStatus(),
                                       std::move(controller));
  } else {
    LogCrossSiteDocumentAction(
        needs_sniffing_
            ? CrossSiteDocumentResourceHandler::Action::kAllowedAfterSniffing
            : CrossSiteDocumentResourceHandler::Action::
                  kAllowedWithoutSniffing);

    next_handler_->OnResponseCompleted(status, std::move(controller));
  }
}

bool CrossSiteDocumentResourceHandler::ShouldBlockBasedOnHeaders(
    ResourceResponse* response) {
  // The checks in this method are ordered to rule out blocking in most cases as
  // quickly as possible.  Checks that are likely to lead to returning false or
  // that are inexpensive should be near the top.
  const GURL& url = request()->url();

  // Check if the response's site needs to have its documents protected.  By
  // default, this will usually return false.
  // TODO(creis): This check can go away once the logic here is made fully
  // backward compatible and we can enforce it always, regardless of Site
  // Isolation policy.
  if (!SiteIsolationPolicy::UseDedicatedProcessesForAllSites() &&
      !ChildProcessSecurityPolicyImpl::GetInstance()->IsIsolatedOrigin(
          url::Origin::Create(url))) {
    return false;
  }

  // Look up MIME type.  If it doesn't claim to be a blockable type (i.e., HTML,
  // XML, JSON, or plain text), don't block it.
  canonical_mime_type_ = CrossSiteDocumentClassifier::GetCanonicalMimeType(
      response->head.mime_type);
  if (canonical_mime_type_ == CROSS_SITE_DOCUMENT_MIME_TYPE_OTHERS)
    return false;

  // Treat a missing initiator as an empty origin to be safe, though we don't
  // expect this to happen.  Unfortunately, this requires a copy.
  url::Origin initiator;
  if (request()->initiator().has_value())
    initiator = request()->initiator().value();

  // Don't block same-site documents.
  if (CrossSiteDocumentClassifier::IsSameSite(initiator, url))
    return false;

  // Only block documents from HTTP(S) schemes.
  if (!CrossSiteDocumentClassifier::IsBlockableScheme(url))
    return false;

  // Allow requests from file:// URLs for now.
  // TODO(creis): Limit this to when the allow_universal_access_from_file_urls
  // preference is set.  See https://crbug.com/789781.
  if (initiator.scheme() == url::kFileScheme)
    return false;

  // Only block if this is a request made from a renderer process.
  const ResourceRequestInfoImpl* info = GetRequestInfo();
  if (!info || info->GetChildID() == -1)
    return false;

  // Give embedder a chance to skip document blocking for this response.
  if (GetContentClient()->browser()->ShouldBypassDocumentBlocking(
          initiator, url, GetRequestInfo()->GetResourceType())) {
    return false;
  }

  // TODO(creis): Don't block plugin requests with universal access. This could
  // be done by allowing resource_type_ == RESOURCE_TYPE_PLUGIN_RESOURCE unless
  // it had an Origin request header and IsValidCorsHeaderSet returned false.
  // (That would matter for plugins without universal access, which use CORS.)

  // Allow the response through if it has valid CORS headers.
  std::string cors_header;
  response->head.headers->GetNormalizedHeader("access-control-allow-origin",
                                              &cors_header);
  if (CrossSiteDocumentClassifier::IsValidCorsHeaderSet(initiator, url,
                                                        cors_header)) {
    return false;
  }

  // We intend to block the response at this point.  However, we will usually
  // sniff the contents to confirm the MIME type, to avoid blocking incorrectly
  // labeled JavaScript, JSONP, etc files.
  //
  // Note: only sniff if there isn't a nosniff header, and if it is not a range
  // request.  Range requests would let an attacker bypass blocking by
  // requesting a range that fails to sniff as a protected type.
  std::string nosniff_header;
  response->head.headers->GetNormalizedHeader("x-content-type-options",
                                              &nosniff_header);
  std::string range_header;
  response->head.headers->GetNormalizedHeader("content-range", &range_header);
  needs_sniffing_ = !base::LowerCaseEqualsASCII(nosniff_header, "nosniff") &&
                    range_header.empty();

  return true;
}

}  // namespace content
