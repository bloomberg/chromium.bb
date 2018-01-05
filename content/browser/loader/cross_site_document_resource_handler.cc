// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/cross_site_document_resource_handler.h"

#include <algorithm>
#include <string.h>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/loader/detachable_resource_handler.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/site_isolation_policy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_sniffer.h"
#include "net/url_request/url_request.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace content {

namespace {

void LogCrossSiteDocumentAction(
    CrossSiteDocumentResourceHandler::Action action) {
  UMA_HISTOGRAM_ENUMERATION("SiteIsolation.XSD.Browser.Action", action,
                            CrossSiteDocumentResourceHandler::Action::kCount);
}

// An IOBuffer to enable writing into a existing IOBuffer at a given offset.
class LocalIoBufferWithOffset : public net::WrappedIOBuffer {
 public:
  LocalIoBufferWithOffset(net::IOBuffer* buf, int offset)
      : net::WrappedIOBuffer(buf->data() + offset), buf_(buf) {}

 private:
  ~LocalIoBufferWithOffset() override {}

  scoped_refptr<net::IOBuffer> buf_;
};

// Helper for the text/plain case.
CrossSiteDocumentClassifier::Result SniffForHtmlXmlOrJson(
    base::StringPiece data) {
  DCHECK_LT(CrossSiteDocumentClassifier::kNo,
            CrossSiteDocumentClassifier::kMaybe);
  auto result = CrossSiteDocumentClassifier::SniffForHTML(data);
  if (result != CrossSiteDocumentClassifier::kYes)
    result = std::max(CrossSiteDocumentClassifier::SniffForXML(data), result);
  if (result != CrossSiteDocumentClassifier::kYes)
    result = std::max(CrossSiteDocumentClassifier::SniffForJSON(data), result);
  return result;
}

}  // namespace

// static
void CrossSiteDocumentResourceHandler::LogBlockedResponseOnUIThread(
    ResourceRequestInfo::WebContentsGetter web_contents_getter,
    bool needed_sniffing,
    CrossSiteDocumentMimeType canonical_mime_type,
    ResourceType resource_type,
    int http_response_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents)
    return;

  ukm::UkmRecorder* recorder = ukm::UkmRecorder::Get();
  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  recorder->UpdateSourceURL(source_id, web_contents->GetLastCommittedURL());
  ukm::builders::SiteIsolation_XSD_Browser_Blocked(source_id)
      .SetContentResourceType(resource_type)
      .SetCanonicalMimeType(canonical_mime_type)
      .SetHttpResponseCode(http_response_code)
      .SetNeededSniffing(needed_sniffing)
      .Record(recorder);
}

// static
void CrossSiteDocumentResourceHandler::LogBlockedResponse(
    ResourceRequestInfoImpl* resource_request_info,
    bool needed_sniffing,
    bool found_parser_breaker,
    CrossSiteDocumentMimeType canonical_mime_type,
    int http_response_code) {
  LogCrossSiteDocumentAction(
      needed_sniffing
          ? CrossSiteDocumentResourceHandler::Action::kBlockedAfterSniffing
          : CrossSiteDocumentResourceHandler::Action::kBlockedWithoutSniffing);

  ResourceType resource_type = resource_request_info->GetResourceType();
  UMA_HISTOGRAM_ENUMERATION("SiteIsolation.XSD.Browser.Blocked", resource_type,
                            content::RESOURCE_TYPE_LAST_TYPE);
  if (found_parser_breaker) {
    UMA_HISTOGRAM_ENUMERATION(
        "SiteIsolation.XSD.Browser.BlockedForParserBreaker", resource_type,
        content::RESOURCE_TYPE_LAST_TYPE);
  }
  switch (canonical_mime_type) {
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
    case CROSS_SITE_DOCUMENT_MIME_TYPE_OTHERS:
      UMA_HISTOGRAM_ENUMERATION("SiteIsolation.XSD.Browser.Blocked.Others",
                                resource_type,
                                content::RESOURCE_TYPE_LAST_TYPE);
      break;
    default:
      NOTREACHED();
  }

  // The last committed URL is only available on the UI thread - we need to hop
  // onto the UI thread to log an UKM event.  Note that this is racey - by the
  // time the posted task runs, the WebContents could have been closed and/or
  // navigated to another URL.  This is understood and acceptable - this should
  // be rare enough to not matter for the collected UKM data.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(
          &CrossSiteDocumentResourceHandler::LogBlockedResponseOnUIThread,
          base::Passed(resource_request_info->GetWebContentsGetterForRequest()),
          needed_sniffing, canonical_mime_type, resource_type,
          http_response_code));
}

// ResourceController that runs a closure on Resume(), and forwards failures
// back to CrossSiteDocumentHandler. The closure can optionally be run as
// a PostTask.
class CrossSiteDocumentResourceHandler::Controller : public ResourceController {
 public:
  explicit Controller(CrossSiteDocumentResourceHandler* document_handler,
                      bool post_task,
                      base::OnceClosure resume_callback)
      : document_handler_(document_handler),
        resume_callback_(std::move(resume_callback)),
        post_task_(post_task) {}

  ~Controller() override {}

  // ResourceController implementation:
  void Resume() override {
    MarkAsUsed();

    if (post_task_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, std::move(resume_callback_));
    } else {
      std::move(resume_callback_).Run();
    }
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

  // Runs on Resume().
  base::OnceClosure resume_callback_;
  bool post_task_;

  DISALLOW_COPY_AND_ASSIGN(Controller);
};

CrossSiteDocumentResourceHandler::CrossSiteDocumentResourceHandler(
    std::unique_ptr<ResourceHandler> next_handler,
    net::URLRequest* request,
    bool is_nocors_plugin_request)
    : LayeredResourceHandler(request, std::move(next_handler)),
      weak_next_handler_(next_handler_.get()),
      is_nocors_plugin_request_(is_nocors_plugin_request),
      weak_this_(this) {}

CrossSiteDocumentResourceHandler::~CrossSiteDocumentResourceHandler() {}

void CrossSiteDocumentResourceHandler::OnResponseStarted(
    ResourceResponse* response,
    std::unique_ptr<ResourceController> controller) {
  has_response_started_ = true;
  http_response_code_ =
      response->head.headers ? response->head.headers->response_code() : 0;
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

  if (local_buffer_) {
    // If |local_buffer_| exists, continue buffering data into the end of it.
    *buf = new LocalIoBufferWithOffset(local_buffer_.get(),
                                       local_buffer_bytes_read_);
    *buf_size = next_handler_buffer_size_ - local_buffer_bytes_read_;
    controller->Resume();
    return;
  }

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
    controller = std::make_unique<Controller>(
        this, false /* post_task */,
        base::BindOnce(&CrossSiteDocumentResourceHandler::ResumeOnWillRead,
                       weak_this_.GetWeakPtr(), buf, buf_size));
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
  // over in one operation.  This will be large, since the MIME sniffing handler
  // is downstream.
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
    bool found_parser_breaker = false;
    auto confirmed_blockable = CrossSiteDocumentClassifier::kNo;
    if (!needs_sniffing_) {
      // If sniffing is impossible (e.g., because this is a range request), or
      // if sniffing is disabled due to a nosniff header AND the server returned
      // a protected mime type, then we have enough information to block
      // immediately.
      confirmed_blockable = CrossSiteDocumentClassifier::kYes;
    } else if (bytes_read == 0) {
      // We haven't blocked the response yet (because previous reads yielded a
      // kMaybe result), and there is no more data. Allow the response.
      confirmed_blockable = CrossSiteDocumentClassifier::kNo;
    } else {
      // Sniff the data to see if it likely matches the MIME type that caused us
      // to decide to block it.  If it doesn't match, it may be JavaScript,
      // JSONP, or another allowable data type and we should let it through.
      // Record how many bytes were read to see how often it's too small.  (This
      // will typically be under 100,000.)
      local_buffer_bytes_read_ += bytes_read;
      DCHECK_LE(local_buffer_bytes_read_, next_handler_buffer_size_);

      // To ensure determinism with respect to network packet ordering and
      // sizing, never examine more than kMaxBytesToSniff bytes, even if more
      // are available.
      size_t bytes_to_sniff =
          std::min(local_buffer_bytes_read_, net::kMaxBytesToSniff);
      base::StringPiece data(local_buffer_->data(), bytes_to_sniff);

      // If the server returned a protected mime type, sniff the response to
      // confirm it.
      if (canonical_mime_type_ == CROSS_SITE_DOCUMENT_MIME_TYPE_HTML) {
        confirmed_blockable = CrossSiteDocumentClassifier::SniffForHTML(data);
      } else if (canonical_mime_type_ == CROSS_SITE_DOCUMENT_MIME_TYPE_XML) {
        confirmed_blockable = CrossSiteDocumentClassifier::SniffForXML(data);
      } else if (canonical_mime_type_ == CROSS_SITE_DOCUMENT_MIME_TYPE_JSON) {
        confirmed_blockable = CrossSiteDocumentClassifier::SniffForJSON(data);
      } else if (canonical_mime_type_ == CROSS_SITE_DOCUMENT_MIME_TYPE_PLAIN) {
        // For responses labeled as plain text, only block them if the data
        // sniffs as one of the formats we would block in the first place.
        confirmed_blockable = SniffForHtmlXmlOrJson(data);
      }

      // Additionally, on all mime types (including _OTHERS), look for
      // Javascript parser breakers. These are affirmative patterns that
      // indicate this resource should only be consumed by XHR/fetch (and we've
      // already verified that this response isn't a permissable cross-origin
      // XHR/fetch).
      if (confirmed_blockable != CrossSiteDocumentClassifier::kYes) {
        auto result =
            CrossSiteDocumentClassifier::SniffForFetchOnlyResource(data);
        found_parser_breaker = (result == CrossSiteDocumentClassifier::kYes);
        confirmed_blockable = std::max(confirmed_blockable, result);
      }

      // If sniffing didn't yield a conclusive response, and we haven't read too
      // many bytes yet, buffer up some more data.
      if (confirmed_blockable == CrossSiteDocumentClassifier::kMaybe &&
          local_buffer_bytes_read_ < net::kMaxBytesToSniff &&
          local_buffer_bytes_read_ < next_handler_buffer_size_) {
        controller->Resume();
        return;
      }
    }

    if (needs_sniffing_) {
      UMA_HISTOGRAM_COUNTS("SiteIsolation.XSD.Browser.BytesReadForSniffing",
                           local_buffer_bytes_read_);
    }

    if (confirmed_blockable == CrossSiteDocumentClassifier::kYes) {
      // Block the response and throw away the data.  Report zero bytes read.
      bytes_read = 0;
      blocked_read_completed_ = true;
      ResourceRequestInfoImpl* info = GetRequestInfo();
      info->set_blocked_cross_site_document(true);

      // Log the blocking event.  Inline the Serialize call to avoid it when
      // tracing is disabled.
      TRACE_EVENT2("navigation",
                   "CrossSiteDocumentResourceHandler::ShouldBlockResponse",
                   "initiator",
                   request()->initiator().has_value()
                       ? request()->initiator().value().Serialize()
                       : "null",
                   "url", request()->url().spec());

      LogBlockedResponse(GetRequestInfo(), needs_sniffing_,
                         found_parser_breaker, canonical_mime_type_,
                         http_response_code_);
    } else {
      // Choose not block this response. Pass the contents of |local_buffer_|
      // onto the next handler. Note that the size of the two buffers is the
      // same (see OnWillRead).
      DCHECK_LE(local_buffer_bytes_read_, next_handler_buffer_size_);
      memcpy(next_handler_buffer_->data(), local_buffer_->data(),
             local_buffer_bytes_read_);
      allow_based_on_sniffing_ = true;

      if (bytes_read == 0 && local_buffer_bytes_read_ != 0) {
        // |bytes_read == 0| indicates the end-of-stream. In this case, we need
        // to synthesize an additional OnWillRead() and OnReadCompleted(0) on
        // |next_handler_|, so that |next_handler_| sees both the full response
        // and the end-of-stream marker. The resulting operations are as
        // follows:
        //
        //  1. next_handler_->OnReadCompleted(bytes_read = contentLength)
        //  2. next_handler_->OnWillRead()  [via PostTask]
        //  3. next_handler_->OnReadCompleted(bytes_read = 0) [via PostTask]
        //  4. controller->Resume()
        HoldController(std::move(controller));
        controller = std::make_unique<Controller>(
            this, true /* post_task */,
            base::BindOnce(
                &ResourceHandler::OnWillRead, weak_next_handler_.GetWeakPtr(),
                &next_handler_buffer_, &next_handler_buffer_size_,
                std::make_unique<Controller>(
                    this, true /* post_task */,
                    base::BindOnce(
                        &ResourceHandler::OnReadCompleted,
                        weak_next_handler_.GetWeakPtr(), 0 /* bytes_read */,
                        std::make_unique<Controller>(
                            this, false /* post_task */,
                            base::BindOnce(
                                &CrossSiteDocumentResourceHandler::Resume,
                                weak_this_.GetWeakPtr()))))));
      }
      bytes_read = local_buffer_bytes_read_;
    }

    // Clean up, whether we'll cancel or proceed from here.
    local_buffer_ = nullptr;
    local_buffer_bytes_read_ = 0;
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
  switch (SiteIsolationPolicy::IsCrossSiteDocumentBlockingEnabled()) {
    case SiteIsolationPolicy::XSDB_ENABLED_UNCONDITIONALLY:
      break;
    case SiteIsolationPolicy::XSDB_ENABLED_IF_ISOLATED:
      if (!SiteIsolationPolicy::UseDedicatedProcessesForAllSites() &&
          !ChildProcessSecurityPolicyImpl::GetInstance()->IsIsolatedOrigin(
              url::Origin::Create(url))) {
        return false;
      }
      break;
    case SiteIsolationPolicy::XSDB_DISABLED:
      return false;
  }

  // Look up MIME type.  Even if it doesn't claim to be a blockable type (i.e.,
  // HTML, XML, JSON, or plain text), it may still fail the checks during the
  // SniffForFetchOnlyResource() phase.
  //
  // TODO(nick): What if the mime type is omitted? Should that be treated the
  // same as text/plain? https://crbug.com/795971
  canonical_mime_type_ = CrossSiteDocumentClassifier::GetCanonicalMimeType(
      response->head.mime_type);

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
          initiator, url, info->GetResourceType())) {
    return false;
  }

  // Allow the response through if it has valid CORS headers.
  std::string cors_header;
  response->head.headers->GetNormalizedHeader("access-control-allow-origin",
                                              &cors_header);
  if (CrossSiteDocumentClassifier::IsValidCorsHeaderSet(initiator, url,
                                                        cors_header)) {
    return false;
  }

  // Don't block plugin requests with universal access (e.g., Flash).  Such
  // requests are made without CORS, and thus dont have an Origin request
  // header.  Other plugin requests (e.g., NaCl) are made using CORS and have an
  // Origin request header.  If they fail the CORS check above, they should be
  // blocked.
  if (info->GetResourceType() == RESOURCE_TYPE_PLUGIN_RESOURCE &&
      is_nocors_plugin_request_) {
    return false;
  }

  // We intend to block the response at this point.  However, we will usually
  // sniff the contents to confirm the MIME type, to avoid blocking incorrectly
  // labeled JavaScript, JSONP, etc files.
  //
  // Note: if there is a nosniff header, it means we should honor the response
  // mime type without trying to confirm it.
  std::string nosniff_header;
  response->head.headers->GetNormalizedHeader("x-content-type-options",
                                              &nosniff_header);
  bool has_nosniff_header =
      base::LowerCaseEqualsASCII(nosniff_header, "nosniff");

  // If this is an HTTP range request, sniffing isn't possible.
  std::string range_header;
  response->head.headers->GetNormalizedHeader("content-range", &range_header);
  bool has_range_header = !range_header.empty();

  // If this is a partial response, sniffing is not possible, so allow the
  // response if it's not a protected mime type.
  if (has_range_header &&
      canonical_mime_type_ == CROSS_SITE_DOCUMENT_MIME_TYPE_OTHERS) {
    return false;
  }

  // We need to sniff unprotected mime types (e.g. for parser breakers), and
  // unless the nosniff header is set, we also need to sniff protected mime
  // types to verify that they're not mislabeled.
  needs_sniffing_ =
      (canonical_mime_type_ == CROSS_SITE_DOCUMENT_MIME_TYPE_OTHERS) ||
      !(has_range_header || has_nosniff_header);

  return true;
}

}  // namespace content
