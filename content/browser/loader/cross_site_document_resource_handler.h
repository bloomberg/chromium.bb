// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_CROSS_SITE_DOCUMENT_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_CROSS_SITE_DOCUMENT_RESOURCE_HANDLER_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "content/browser/loader/layered_resource_handler.h"
#include "content/common/cross_site_document_classifier.h"
#include "content/public/common/resource_type.h"

namespace net {
class URLRequest;
}  // namespace net

namespace content {

// A ResourceHandler that prevents the renderer process from receiving network
// responses that contain cross-site documents (HTML, XML, some plain text) or
// similar data that should be opaque (JSON), with appropriate exceptions to
// preserve compatibility.  Other cross-site resources such as scripts, images,
// stylesheets, etc are still allowed.
//
// This handler is not used for navigations, which create a new security context
// based on the origin of the response.  It currently only protects documents
// from sites that require dedicated renderer processes, though it could be
// expanded to apply to all sites.
//
// When a response is blocked, the renderer is sent an empty response body
// instead of seeing a failed request.  A failed request would change page-
// visible behavior (e.g., for a blocked XHR).  An empty response can generally
// be consumed by the renderer without noticing the difference.
//
// For more details, see:
// http://chromium.org/developers/design-documents/blocking-cross-site-documents
class CONTENT_EXPORT CrossSiteDocumentResourceHandler
    : public LayeredResourceHandler {
 public:
  // This enum backs a histogram. Update enums.xml if you make any updates, and
  // put new entries before |kCount|.
  enum class Action {
    // Logged at OnResponseStarted.
    kResponseStarted,

    // Logged when a response is blocked without requiring sniffing.
    kBlockedWithoutSniffing,

    // Logged when a response is blocked as a result of sniffing the content.
    kBlockedAfterSniffing,

    // Logged when a response is allowed without requiring sniffing.
    kAllowedWithoutSniffing,

    // Logged when a response is allowed as a result of sniffing the content.
    kAllowedAfterSniffing,

    kCount
  };

  CrossSiteDocumentResourceHandler(
      std::unique_ptr<ResourceHandler> next_handler,
      net::URLRequest* request);
  ~CrossSiteDocumentResourceHandler() override;

  // LayeredResourceHandler overrides:
  void OnResponseStarted(
      ResourceResponse* response,
      std::unique_ptr<ResourceController> controller) override;
  void OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size,
                  std::unique_ptr<ResourceController> controller) override;
  void OnReadCompleted(int bytes_read,
                       std::unique_ptr<ResourceController> controller) override;
  void OnResponseCompleted(
      const net::URLRequestStatus& status,
      std::unique_ptr<ResourceController> controller) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(CrossSiteDocumentResourceHandlerTest,
                           ResponseBlocking);
  FRIEND_TEST_ALL_PREFIXES(CrossSiteDocumentResourceHandlerTest,
                           OnWillReadDefer);

  // ResourceController that manages the read buffer if a downstream handler
  // defers during OnWillRead.
  class OnWillReadController;

  // Computes whether this response contains a cross-site document that needs to
  // be blocked from the renderer process.  This is a first approximation based
  // on the headers, and may be revised after some of the data is sniffed.
  bool ShouldBlockBasedOnHeaders(ResourceResponse* response);

  // Once the downstream handler has allocated the buffer for OnWillRead
  // (possibly after deferring), this sets up sniffing into a local buffer.
  // Called by the OnWillReadController.
  void ResumeOnWillRead(scoped_refptr<net::IOBuffer>* buf, int* buf_size);

  // A local buffer for sniffing content and using for throwaway reads.
  // This is not shared with the renderer process.
  scoped_refptr<net::IOBuffer> local_buffer_;

  // The buffer allocated by the next ResourceHandler for reads, which is used
  // if sniffing determines that we should proceed with the response.
  scoped_refptr<net::IOBuffer> next_handler_buffer_;

  // The size of |next_handler_buffer_|.
  int next_handler_buffer_size_ = 0;

  // A canonicalization of the specified MIME type, to determine if blocking the
  // response is needed, as well as which type of sniffing to perform.
  CrossSiteDocumentMimeType canonical_mime_type_ =
      CROSS_SITE_DOCUMENT_MIME_TYPE_OTHERS;

  // Tracks whether OnResponseStarted has been called, to ensure that it happens
  // before OnWillRead and OnReadCompleted.
  bool has_response_started_ = false;

  // Whether this response is a cross-site document that should be blocked,
  // pending the outcome of sniffing the content.  Set in OnResponseStarted and
  // should only be read afterwards.
  bool should_block_based_on_headers_ = false;

  // Whether the response data should be sniffed before blocking it, to avoid
  // blocking mislabeled responses (e.g., JSONP labeled as HTML).  This is
  // usually true when |should_block_based_on_headers_| is set, unless there is
  // a nosniff header or range request.
  bool needs_sniffing_ = false;

  // Whether this response will be allowed through despite being flagged for
  // blocking (via |should_block_based_on_headers_), because sniffing determined
  // it was incorrectly labeled and might be needed for compatibility (e.g.,
  // in case it is Javascript).
  bool allow_based_on_sniffing_ = false;

  // Whether the next ResourceHandler has already been told that the read has
  // completed, and thus it is safe to cancel or detach on the next read.
  bool blocked_read_completed_ = false;

  DISALLOW_COPY_AND_ASSIGN(CrossSiteDocumentResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_CROSS_SITE_DOCUMENT_RESOURCE_HANDLER_H_
