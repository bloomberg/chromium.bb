// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/cross_site_document_resource_handler.h"

#include <string.h>

#include <algorithm>
#include <string>
#include <unordered_set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
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

using MimeType = network::CrossOriginReadBlocking::MimeType;
using SniffingResult = network::CrossOriginReadBlocking::Result;

namespace content {

// An interface to enable incremental content sniffing. These are instantiated
// for each each request; thus they can be stateful.
class CrossSiteDocumentResourceHandler::ConfirmationSniffer {
 public:
  virtual ~ConfirmationSniffer() = default;

  // Called after data is read from the network. |sniffing_buffer| contains the
  // entire response body delivered thus far. To support streaming,
  // |new_data_offset| gives the offset into |sniffing_buffer| at which new data
  // was appended since the last read.
  virtual void OnDataAvailable(base::StringPiece sniffing_buffer,
                               size_t new_data_offset) = 0;

  // Returns true if the return value of IsConfirmedContentType() might change
  // with the addition of more data. Returns false if a final decision is
  // available.
  virtual bool WantsMoreData() const = 0;

  // Returns true if the data has been confirmed to be of the CORB-protected
  // content type that this sniffer is intended to detect.
  virtual bool IsConfirmedContentType() const = 0;

  // Called when this sniffer's decision was used to block a response. This will
  // only be invoked when an earlier call to IsConfirmedContentType() returned
  // true.
  virtual void LogBlockedResponse(ResourceType resource_type) const {}
};

namespace {

void LogCrossSiteDocumentAction(
    CrossSiteDocumentResourceHandler::Action action) {
  UMA_HISTOGRAM_ENUMERATION("SiteIsolation.XSD.Browser.Action", action,
                            CrossSiteDocumentResourceHandler::Action::kCount);
}

// A ConfirmationSniffer that wraps one of the sniffing functions from
// network::CrossOriginReadBlocking.
class SimpleConfirmationSniffer
    : public CrossSiteDocumentResourceHandler::ConfirmationSniffer {
 public:
  // The function pointer type corresponding to one of the available sniffing
  // functions from network::CrossOriginReadBlocking.
  using SnifferFunction =
      decltype(&network::CrossOriginReadBlocking::SniffForHTML);

  explicit SimpleConfirmationSniffer(SnifferFunction sniffer_function)
      : sniffer_function_(sniffer_function) {}
  ~SimpleConfirmationSniffer() override = default;

  void OnDataAvailable(base::StringPiece sniffing_buffer,
                       size_t new_data_offset) final {
    DCHECK_LE(new_data_offset, sniffing_buffer.length());
    if (new_data_offset == sniffing_buffer.length()) {
      // No new data -- do nothing. This happens at end-of-stream.
      return;
    }
    // The sniffing functions don't support streaming, so with each new chunk of
    // data, call the sniffer on the whole buffer.
    last_sniff_result_ = (*sniffer_function_)(sniffing_buffer);
  }

  bool WantsMoreData() const final {
    // kNo and kYes results are final, meaning that sniffing can stop once they
    // occur. A kMaybe result corresponds to an indeterminate state, that could
    // change to kYes or kNo with more data.
    return last_sniff_result_ == SniffingResult::kMaybe;
  }

  bool IsConfirmedContentType() const final {
    // Only confirm the mime type if an affirmative pattern (e.g. an HTML tag,
    // if using the HTML sniffer) was detected.
    //
    // Note that if the stream ends (or net::kMaxBytesToSniff has been reached)
    // and |last_sniff_result_| is kMaybe, the response is allowed to go
    // through.
    return last_sniff_result_ == SniffingResult::kYes;
  }

 private:
  // The function that actually knows how to sniff for a content type.
  SnifferFunction sniffer_function_;

  // Result of sniffing the data available thus far.
  SniffingResult last_sniff_result_ = SniffingResult::kMaybe;
};

// A ConfirmationSniffer for parser breakers (fetch-only resources). This logs
// to an UMA histogram whenever it is the reason for a response being blocked.
class FetchOnlyResourceSniffer : public SimpleConfirmationSniffer {
 public:
  FetchOnlyResourceSniffer()
      : SimpleConfirmationSniffer(
            &network::CrossOriginReadBlocking::SniffForFetchOnlyResource) {}

  void LogBlockedResponse(ResourceType resource_type) const final {
    UMA_HISTOGRAM_ENUMERATION(
        "SiteIsolation.XSD.Browser.BlockedForParserBreaker", resource_type,
        content::RESOURCE_TYPE_LAST_TYPE);
  }
};

// An IOBuffer to enable writing into a existing IOBuffer at a given offset.
class LocalIoBufferWithOffset : public net::WrappedIOBuffer {
 public:
  LocalIoBufferWithOffset(net::IOBuffer* buf, int offset)
      : net::WrappedIOBuffer(buf->data() + offset), buf_(buf) {}

 private:
  ~LocalIoBufferWithOffset() override {}

  scoped_refptr<net::IOBuffer> buf_;
};

// Headers from
// https://fetch.spec.whatwg.org/#cors-safelisted-response-header-name.
//
// Note that XSDB doesn't block responses allowed through CORS - this means
// that the list of allowed headers below doesn't have to consider header
// names listed in the Access-Control-Expose-Headers header.
const char* const kCorsSafelistedHeaders[] = {
    "cache-control", "content-language", "content-type",
    "expires",       "last-modified",    "pragma",
};

// Removes headers that should be blocked in cross-origin case.
// See https://fetch.spec.whatwg.org/#cors-safelisted-response-header-name.
void SanitizeResponseHeaders(
    const scoped_refptr<net::HttpResponseHeaders>& headers) {
  DCHECK(headers);
  std::unordered_set<std::string> names_of_headers_to_remove;

  size_t it = 0;
  std::string name;
  std::string value;
  while (headers->EnumerateHeaderLines(&it, &name, &value)) {
    // Don't remove CORS headers - doing so would lead to incorrect error
    // messages for CORS-blocked responses (e.g. Blink would say "[...] No
    // 'Access-Control-Allow-Origin' header is present [...]" instead of saying
    // something like "[...] Access-Control-Allow-Origin' header has a value
    // 'http://www2.localhost:8000' that is not equal to the supplied origin
    // [...]").
    if (base::StartsWith(name, "Access-Control-",
                         base::CompareCase::INSENSITIVE_ASCII)) {
      continue;
    }

    // Remove all other headers (but note the final exclusion below).
    names_of_headers_to_remove.insert(base::ToLowerASCII(name));
  }

  // Exclude from removals headers from
  // https://fetch.spec.whatwg.org/#cors-safelisted-response-header-name.
  for (const char* header : kCorsSafelistedHeaders)
    names_of_headers_to_remove.erase(header);

  headers->RemoveHeaders(names_of_headers_to_remove);
}

// Sanitizes/strips metadata of a response we decided to block.
void SanitizeResourceResponse(
    const scoped_refptr<network::ResourceResponse>& response) {
  DCHECK(response);
  response->head.content_length = 0;
  if (response->head.headers)
    SanitizeResponseHeaders(response->head.headers);
}

}  // namespace

// static
void CrossSiteDocumentResourceHandler::LogBlockedResponseOnUIThread(
    ResourceRequestInfo::WebContentsGetter web_contents_getter,
    bool needed_sniffing,
    MimeType canonical_mime_type,
    ResourceType resource_type,
    int http_response_code,
    int64_t content_length) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents = std::move(web_contents_getter).Run();
  if (!web_contents)
    return;

  ukm::UkmRecorder* recorder = ukm::UkmRecorder::Get();
  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  recorder->UpdateSourceURL(source_id, web_contents->GetLastCommittedURL());
  ukm::builders::SiteIsolation_XSD_Browser_Blocked(source_id)
      .SetCanonicalMimeType(static_cast<int64_t>(canonical_mime_type))
      .SetContentLengthWasZero(content_length == 0)
      .SetContentResourceType(resource_type)
      .SetHttpResponseCode(http_response_code)
      .SetNeededSniffing(needed_sniffing)
      .Record(recorder);
}

// static
void CrossSiteDocumentResourceHandler::LogBlockedResponse(
    ResourceRequestInfoImpl* resource_request_info,
    bool needed_sniffing,
    MimeType canonical_mime_type,
    int http_response_code,
    int64_t content_length) {
  DCHECK(resource_request_info);
  DCHECK_NE(network::CrossOriginReadBlocking::MimeType::kInvalid,
            canonical_mime_type);

  LogCrossSiteDocumentAction(
      needed_sniffing
          ? CrossSiteDocumentResourceHandler::Action::kBlockedAfterSniffing
          : CrossSiteDocumentResourceHandler::Action::kBlockedWithoutSniffing);

  UMA_HISTOGRAM_BOOLEAN(
      "SiteIsolation.XSD.Browser.Blocked.ContentLength.WasAvailable",
      content_length >= 0);
  if (content_length >= 0) {
    UMA_HISTOGRAM_COUNTS_10000(
        "SiteIsolation.XSD.Browser.Blocked.ContentLength.ValueIfAvailable",
        content_length);
  }

  ResourceType resource_type = resource_request_info->GetResourceType();
  UMA_HISTOGRAM_ENUMERATION("SiteIsolation.XSD.Browser.Blocked", resource_type,
                            content::RESOURCE_TYPE_LAST_TYPE);
  switch (canonical_mime_type) {
    case MimeType::kHtml:
      UMA_HISTOGRAM_ENUMERATION("SiteIsolation.XSD.Browser.Blocked.HTML",
                                resource_type,
                                content::RESOURCE_TYPE_LAST_TYPE);
      break;
    case MimeType::kXml:
      UMA_HISTOGRAM_ENUMERATION("SiteIsolation.XSD.Browser.Blocked.XML",
                                resource_type,
                                content::RESOURCE_TYPE_LAST_TYPE);
      break;
    case MimeType::kJson:
      UMA_HISTOGRAM_ENUMERATION("SiteIsolation.XSD.Browser.Blocked.JSON",
                                resource_type,
                                content::RESOURCE_TYPE_LAST_TYPE);
      break;
    case MimeType::kPlain:
      UMA_HISTOGRAM_ENUMERATION("SiteIsolation.XSD.Browser.Blocked.Plain",
                                resource_type,
                                content::RESOURCE_TYPE_LAST_TYPE);
      break;
    case MimeType::kOthers:
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
          resource_request_info->GetWebContentsGetterForRequest(),
          needed_sniffing, canonical_mime_type, resource_type,
          http_response_code, content_length));
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
    network::ResourceResponse* response,
    std::unique_ptr<ResourceController> controller) {
  content_length_ = response->head.content_length;
  has_response_started_ = true;
  http_response_code_ =
      response->head.headers ? response->head.headers->response_code() : 0;

  LogCrossSiteDocumentAction(
      CrossSiteDocumentResourceHandler::Action::kResponseStarted);

  should_block_based_on_headers_ = ShouldBlockBasedOnHeaders(response);

  // If blocking is possible, postpone forwarding |response| to the
  // |next_handler_|, until we have made the allow-vs-block decision
  // (which might need more time depending on |needs_sniffing_|).
  if (should_block_based_on_headers_) {
    pending_response_start_ = response;
    controller->Resume();
  } else {
    next_handler_->OnResponseStarted(response, std::move(controller));
  }
}

void CrossSiteDocumentResourceHandler::OnWillRead(
    scoped_refptr<net::IOBuffer>* buf,
    int* buf_size,
    std::unique_ptr<ResourceController> controller) {
  // For allowed responses, the data is directly streamed to the next handler.
  // Note that OnWillRead may be called before OnResponseStarted (because the
  // MimeSniffingResourceHandler upstream changes the order of the calls) - this
  // means that |has_response_started_| has to be explicitly checked below.
  if (has_response_started_ &&
      (!should_block_based_on_headers_ || allow_based_on_sniffing_)) {
    DCHECK(!local_buffer_);
    next_handler_->OnWillRead(buf, buf_size, std::move(controller));
    return;
  }

  // If |local_buffer_| exists, continue buffering data into the end of it.
  if (local_buffer_) {
    // Check that we still have room for more local bufferring.
    DCHECK_GT(next_handler_buffer_size_, local_buffer_bytes_read_);
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

  // If we haven't yet decided to allow or block the response, we should read
  // the data into a local buffer 1) to temporarily prevent the data from
  // reaching the renderer and 2) to potentially sniff the data to confirm the
  // content type.
  //
  // Since the downstream handler may defer during the OnWillRead call below,
  // the values of |buf| and |buf_size| may not be available right away.
  // Instead, create a Controller that will start the sniffing after the
  // downstream handler has called Resume on it.
  HoldController(std::move(controller));
  controller = std::make_unique<Controller>(
      this, false /* post_task */,
      base::BindOnce(&CrossSiteDocumentResourceHandler::ResumeOnWillRead,
                     weak_this_.GetWeakPtr(), buf, buf_size));
  next_handler_->OnWillRead(buf, buf_size, std::move(controller));
}

void CrossSiteDocumentResourceHandler::ResumeOnWillRead(
    scoped_refptr<net::IOBuffer>* buf,
    int* buf_size) {
  // We should only get here if we haven't yet made a block-vs-allow decision
  // (we get here after downstream handler finishes its work from OnWillRead).
  DCHECK(!allow_based_on_sniffing_);
  DCHECK(!blocked_read_completed_);

  // For most blocked responses, we need to sniff the data to confirm it looks
  // like the claimed MIME type (to avoid blocking mislabeled JavaScript,
  // JSONP, etc).  Read this data into a separate buffer (not shared with the
  // renderer), which we will only copy over if we decide to allow it through.
  // This is only done when we suspect the response should be blocked.
  //
  // Make it as big as the downstream handler's buffer to make it easy to copy
  // over in one operation.
  DCHECK_GE(*buf_size, net::kMaxBytesToSniff * 2);
  local_buffer_ =
      base::MakeRefCounted<net::IOBuffer>(static_cast<size_t>(*buf_size));

  // Store the next handler's buffer but don't read into it while sniffing,
  // since we possibly won't want to send the data to the renderer process.
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

  if (!should_block_based_on_headers_) {
    // CrossSiteDocumentResourceHandler always intercepts the buffer allocated
    // by the first call to |next_handler_|'s OnWillRead and passes the
    // |local_buffer_| upstream.  If we decide not to block based on headers,
    // then the data needs to be passed into the |next_handler_|.
    if (local_buffer_) {
      DCHECK_EQ(0, local_buffer_bytes_read_);
      local_buffer_bytes_read_ = bytes_read;
      StopLocalBuffering(true /* = copy_data_to_next_handler */);
    }

    next_handler_->OnReadCompleted(bytes_read, std::move(controller));
    return;
  }

  if (allow_based_on_sniffing_) {
    // If CrossSiteDocumentResourceHandler decided to allow the response based
    // on sniffing, then StopLocalBuffering was already called below by the
    // previous execution of CrossSiteDocumentResourceHandler::OnReadCompleted.
    // From there onward, we just need to foward all the calls to the
    // |next_handler_|.
    DCHECK(!local_buffer_);
    next_handler_->OnReadCompleted(bytes_read, std::move(controller));
    return;
  }

  // If we intended to block the response and haven't sniffed yet, try to
  // confirm that we should block it.  If sniffing is needed, look at the local
  // buffer and either report that zero bytes were read (to indicate the
  // response is empty and complete), or copy the sniffed data to the next
  // handler's buffer and resume the response without blocking.
  bool confirmed_blockable = false;
  ResourceRequestInfoImpl* info = GetRequestInfo();
  if (!needs_sniffing_) {
    // If sniffing is impossible (e.g., because this is a range request), or
    // if sniffing is disabled due to a nosniff header AND the server returned
    // a protected mime type, then we have enough information to block
    // immediately.
    confirmed_blockable = true;
  } else {
    // Sniff the data to see if it likely matches the MIME type that caused us
    // to decide to block it.  If it doesn't match, it may be JavaScript,
    // JSONP, or another allowable data type and we should let it through.
    // Record how many bytes were read to see how often it's too small.  (This
    // will typically be under 100,000.)
    const size_t new_data_offset = local_buffer_bytes_read_;
    local_buffer_bytes_read_ += bytes_read;
    DCHECK_LE(local_buffer_bytes_read_, next_handler_buffer_size_);
    const bool more_data_possible =
        bytes_read != 0 && local_buffer_bytes_read_ < net::kMaxBytesToSniff &&
        local_buffer_bytes_read_ < next_handler_buffer_size_;

    // To ensure determinism with respect to network packet ordering and
    // sizing, never examine more than kMaxBytesToSniff bytes, even if more
    // are available.
    size_t bytes_to_sniff =
        std::min(local_buffer_bytes_read_, net::kMaxBytesToSniff);
    base::StringPiece data(local_buffer_->data(), bytes_to_sniff);

    for (size_t i = 0; i < sniffers_.size();) {
      sniffers_[i]->OnDataAvailable(data, new_data_offset);

      if (!more_data_possible || !sniffers_[i]->WantsMoreData()) {
        if (sniffers_[i]->IsConfirmedContentType()) {
          // We're done sniffing; this response is CORB-protected.
          confirmed_blockable = true;
          sniffers_[i]->LogBlockedResponse(info->GetResourceType());
          break;
        }

        // This response is CORB-exempt as far as this sniffer is concerned;
        // remove it from the list.
        sniffers_.erase(sniffers_.begin() + i);
      } else {
        i++;
      }
    }

    // When there are no sniffers left, the response is allowed.
    const bool confirmed_allowed = sniffers_.empty();
    DCHECK(!(confirmed_blockable && confirmed_allowed));
    DCHECK(confirmed_blockable || confirmed_allowed || more_data_possible);

    // If sniffing didn't yield a conclusive response, and we haven't read too
    // many bytes yet or hit the end of the stream, buffer up some more data.
    if (!(confirmed_blockable || confirmed_allowed) && more_data_possible) {
      controller->Resume();
      return;
    }
  }

  if (needs_sniffing_) {
    UMA_HISTOGRAM_COUNTS("SiteIsolation.XSD.Browser.BytesReadForSniffing",
                         local_buffer_bytes_read_);
  }

  // At this point we have already made a block-vs-allow decision and we know
  // that we can wake the |next_handler_| and let it catch-up with our
  // processing of the response.  The first step will always be calling
  // |next_handler_->OnResponseStarted(...)|, but we need to figure out what
  // other steps need to happen, before we can resume the real response
  // upstream.  These steps will be gathered into |controller|.
  // The last step will always be calling
  // CrossSiteDocumentResourceHandler::Resume.
  HoldController(std::move(controller));
  controller = std::make_unique<Controller>(
      this, false /* post_task */,
      base::BindOnce(&CrossSiteDocumentResourceHandler::Resume,
                     weak_this_.GetWeakPtr()));

  if (confirmed_blockable) {
    // Log the blocking event.  Inline the Serialize call to avoid it when
    // tracing is disabled.
    TRACE_EVENT2("navigation",
                 "CrossSiteDocumentResourceHandler::ShouldBlockResponse",
                 "initiator",
                 request()->initiator().has_value()
                     ? request()->initiator().value().Serialize()
                     : "null",
                 "url", request()->url().spec());

    LogBlockedResponse(info, needs_sniffing_, canonical_mime_type_,
                       http_response_code_, content_length_);

    // Block the response and throw away the data.  Report zero bytes read.
    blocked_read_completed_ = true;
    info->set_blocked_cross_site_document(true);
    SanitizeResourceResponse(pending_response_start_);

    // Pass an empty/blocked body onto the next handler.  size of the two
    // buffers is the same (see OnWillRead).  After the next statement,
    // |controller| will store a sequence of steps like this:
    //  - next_handler_->OnReadCompleted(bytes_read = 0)
    //  - ... steps from the old |controller| (typically this->Resume()) ...
    controller = std::make_unique<Controller>(
        this, true /* post_task */,
        base::BindOnce(&ResourceHandler::OnReadCompleted,
                       weak_next_handler_.GetWeakPtr(), 0 /* bytes_read */,
                       std::move(controller)));
    StopLocalBuffering(false /* = copy_data_to_next_handler*/);
  } else {
    // Choose not block this response.
    allow_based_on_sniffing_ = true;

    if (bytes_read == 0 && local_buffer_bytes_read_ != 0) {
      // |bytes_read == 0| indicates the end-of-stream. In this case, we need
      // to synthesize an additional OnWillRead() and OnReadCompleted(0) on
      // |next_handler_|, so that |next_handler_| sees both the full response
      // and the end-of-stream marker.  After the next statement, |controller|
      // will store a sequence of steps like this:
      //  - next_handler_->OnWillRead(...)
      //  - next_handler_->OnReadCompleted(bytes_read = 0)
      //  - ... steps from the old |controller| (typically this->Resume()) ...
      //
      // Note that if |weak_next_handler_| is alive, then |this| should also be
      // alive and therefore it is safe to dereference |&next_handler_buffer_|
      // and |&next_handler_buffer_size_|.
      controller = std::make_unique<Controller>(
          this, false /* post_task */,
          base::BindOnce(
              &ResourceHandler::OnWillRead, weak_next_handler_.GetWeakPtr(),
              &next_handler_buffer_, &next_handler_buffer_size_,
              std::make_unique<Controller>(
                  this, true /* post_task */,
                  base::BindOnce(&ResourceHandler::OnReadCompleted,
                                 weak_next_handler_.GetWeakPtr(),
                                 0 /* bytes_read */, std::move(controller)))));
    }

    // Pass the contents of |local_buffer_| onto the next handler.  Afterwards,
    // |controller| will store a sequence of steps like this:
    //  - next_handler_->OnReadCompleted(local_buffer_bytes_read_)
    //  - ... steps from the old |controller| ...
    controller = std::make_unique<Controller>(
        this, true /* post_task */,
        base::BindOnce(&ResourceHandler::OnReadCompleted,
                       weak_next_handler_.GetWeakPtr(),
                       local_buffer_bytes_read_, std::move(controller)));
    StopLocalBuffering(true /* = copy_data_to_next_handler*/);
  }

  // In both the blocked and allowed cases, we need to resume by notifying the
  // downstream handler about the response start.
  DCHECK(pending_response_start_);
  next_handler_->OnResponseStarted(pending_response_start_.get(),
                                   std::move(controller));
  pending_response_start_ = nullptr;
}

void CrossSiteDocumentResourceHandler::StopLocalBuffering(
    bool copy_data_to_next_handler) {
  DCHECK(has_response_started_);
  DCHECK(!should_block_based_on_headers_ || allow_based_on_sniffing_ ||
         blocked_read_completed_);
  DCHECK(local_buffer_);
  DCHECK(next_handler_buffer_);

  if (copy_data_to_next_handler) {
    // Pass the contents of |local_buffer_| onto the next handler. Note that the
    // size of the two buffers is the same (see OnWillRead).
    DCHECK_LE(local_buffer_bytes_read_, next_handler_buffer_size_);
    memcpy(next_handler_buffer_->data(), local_buffer_->data(),
           local_buffer_bytes_read_);
  }

  local_buffer_ = nullptr;
  local_buffer_bytes_read_ = 0;
  next_handler_buffer_ = nullptr;
  next_handler_buffer_size_ = 0;
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
    // Only report XSDB status for successful (i.e. non-aborted,
    // non-errored-out) requests.
    if (status.is_success()) {
      LogCrossSiteDocumentAction(
          needs_sniffing_
              ? CrossSiteDocumentResourceHandler::Action::kAllowedAfterSniffing
              : CrossSiteDocumentResourceHandler::Action::
                    kAllowedWithoutSniffing);
    }

    next_handler_->OnResponseCompleted(status, std::move(controller));
  }
}

bool CrossSiteDocumentResourceHandler::ShouldBlockBasedOnHeaders(
    network::ResourceResponse* response) {
  // The checks in this method are ordered to rule out blocking in most cases as
  // quickly as possible.  Checks that are likely to lead to returning false or
  // that are inexpensive should be near the top.
  const GURL& url = request()->url();
  url::Origin target_origin = url::Origin::Create(url);

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
              target_origin)) {
        return false;
      }
      break;
    case SiteIsolationPolicy::XSDB_DISABLED:
      return false;
  }

  // Treat a missing initiator as an empty origin to be safe, though we don't
  // expect this to happen.  Unfortunately, this requires a copy.
  url::Origin initiator;
  if (request()->initiator().has_value())
    initiator = request()->initiator().value();

  // Don't block same-origin documents.
  if (initiator.IsSameOriginWith(target_origin))
    return false;

  // Only block documents from HTTP(S) schemes.  Checking the scheme of
  // |target_origin| ensures that we also protect content of blob: and
  // filesystem: URLs if their nested origins have a HTTP(S) scheme.
  if (!network::CrossOriginReadBlocking::IsBlockableScheme(
          target_origin.GetURL()))
    return false;

  // Allow requests from file:// URLs for now.
  // TODO(creis): Limit this to when the allow_universal_access_from_file_urls
  // preference is set.  See https://crbug.com/789781.
  if (initiator.scheme() == url::kFileScheme)
    return false;

  // Give embedder a chance to skip document blocking for this response.
  const char* initiator_scheme_exception =
      GetContentClient()
          ->browser()
          ->GetInitatorSchemeBypassingDocumentBlocking();
  if (initiator_scheme_exception &&
      initiator.scheme() == initiator_scheme_exception) {
    return false;
  }

  // Only block if this is a request made from a renderer process.
  const ResourceRequestInfoImpl* info = GetRequestInfo();
  if (!info || info->GetChildID() == -1)
    return false;

  // Allow the response through if it has valid CORS headers.
  std::string cors_header;
  response->head.headers->GetNormalizedHeader("access-control-allow-origin",
                                              &cors_header);
  if (network::CrossOriginReadBlocking::IsValidCorsHeaderSet(initiator,
                                                             cors_header)) {
    return false;
  }

  // Requests from foo.example.com will consult foo.example.com's service worker
  // first (if one has been registered).  The service worker can handle requests
  // initiated by foo.example.com even if they are cross-origin (e.g. requests
  // for bar.example.com).  This is okay and should not be blocked by XSDB,
  // unless the initiator opted out of CORS / opted into receiving an opaque
  // response.  See also https://crbug.com/803672.
  if (response->head.was_fetched_via_service_worker) {
    switch (response->head.response_type_via_service_worker) {
      case network::mojom::FetchResponseType::kBasic:
      case network::mojom::FetchResponseType::kCORS:
      case network::mojom::FetchResponseType::kDefault:
      case network::mojom::FetchResponseType::kError:
        // Non-opaque responses shouldn't be blocked.
        return false;
      case network::mojom::FetchResponseType::kOpaque:
      case network::mojom::FetchResponseType::kOpaqueRedirect:
        // Opaque responses are eligible for blocking. Continue on...
        break;
    }
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

  // CORB should look directly at the Content-Type header if one has been
  // received from the network.  Ignoring |response->head.mime_type| helps avoid
  // breaking legitimate websites (which might happen more often when blocking
  // would be based on the mime type sniffed by MimeSniffingResourceHandler).
  //
  // TODO(nick): What if the mime type is omitted? Should that be treated the
  // same as text/plain? https://crbug.com/795971
  std::string mime_type;
  if (response->head.headers)
    response->head.headers->GetMimeType(&mime_type);
  // Canonicalize the MIME type.  Note that even if it doesn't claim to be a
  // blockable type (i.e., HTML, XML, JSON, or plain text), it may still fail
  // the checks during the SniffForFetchOnlyResource() phase.
  canonical_mime_type_ =
      network::CrossOriginReadBlocking::GetCanonicalMimeType(mime_type);

  // If this is a partial response, sniffing is not possible, so allow the
  // response if it's not a protected mime type.
  if (has_range_header && canonical_mime_type_ == MimeType::kOthers) {
    return false;
  }

  // We need to sniff unprotected mime types (e.g. for parser breakers), and
  // unless the nosniff header is set, we also need to sniff protected mime
  // types to verify that they're not mislabeled.
  needs_sniffing_ = (canonical_mime_type_ == MimeType::kOthers) ||
                    !(has_range_header || has_nosniff_header);

  // Stylesheets shouldn't be sniffed for JSON parser breakers - see
  // https://crbug.com/809259.
  if (response->head.mime_type.empty() ||
      base::LowerCaseEqualsASCII(response->head.mime_type, "text/css")) {
    return false;
  }

  // Create one or more |sniffers_| to confirm that the body is actually the
  // MIME type advertised in the Content-Type header.
  if (needs_sniffing_) {
    // When the MIME type is "text/plain", create sniffers for HTML, XML and
    // JSON. If any of these sniffers match, the response will be blocked.
    const bool use_all = canonical_mime_type_ == MimeType::kPlain;

    // HTML sniffer.
    if (use_all || canonical_mime_type_ == MimeType::kHtml) {
      sniffers_.push_back(std::make_unique<SimpleConfirmationSniffer>(
          &network::CrossOriginReadBlocking::SniffForHTML));
    }

    // XML sniffer.
    if (use_all || canonical_mime_type_ == MimeType::kXml) {
      sniffers_.push_back(std::make_unique<SimpleConfirmationSniffer>(
          &network::CrossOriginReadBlocking::SniffForXML));
    }

    // JSON sniffer.
    if (use_all || canonical_mime_type_ == MimeType::kJson) {
      sniffers_.push_back(std::make_unique<SimpleConfirmationSniffer>(
          &network::CrossOriginReadBlocking::SniffForJSON));
    }

    // Parser-breaker sniffer.
    //
    // Because these prefixes are an XSSI-defeating mechanism, CORB considers
    // them distinctive enough to be worth blocking no matter the Content-Type
    // header. So this sniffer is created unconditionally.
    //
    // For MimeType::kOthers, this will be the only sniffer that's active.
    sniffers_.push_back(std::make_unique<FetchOnlyResourceSniffer>());
  }

  return true;
}

// static
std::vector<std::string>
CrossSiteDocumentResourceHandler::GetCorsSafelistedHeadersForTesting() {
  return std::vector<std::string>(
      kCorsSafelistedHeaders,
      kCorsSafelistedHeaders + arraysize(kCorsSafelistedHeaders));
}

}  // namespace content
