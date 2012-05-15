// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/buffered_resource_handler.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "content/browser/download/download_resource_handler.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/plugin_service_impl.h"
#include "content/browser/renderer_host/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/resource_request_info_impl.h"
#include "content/browser/renderer_host/x509_user_cert_resource_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/common/resource_response.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_sniffer.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_content_disposition.h"
#include "net/http/http_response_headers.h"
#include "webkit/plugins/webplugininfo.h"

namespace content {

namespace {

void RecordSnifferMetrics(bool sniffing_blocked,
                          bool we_would_like_to_sniff,
                          const std::string& mime_type) {
  static base::Histogram* nosniff_usage(NULL);
  if (!nosniff_usage)
    nosniff_usage = base::BooleanHistogram::FactoryGet(
        "nosniff.usage", base::Histogram::kUmaTargetedHistogramFlag);
  nosniff_usage->AddBoolean(sniffing_blocked);

  if (sniffing_blocked) {
    static base::Histogram* nosniff_otherwise(NULL);
    if (!nosniff_otherwise)
      nosniff_otherwise = base::BooleanHistogram::FactoryGet(
          "nosniff.otherwise", base::Histogram::kUmaTargetedHistogramFlag);
    nosniff_otherwise->AddBoolean(we_would_like_to_sniff);

    static base::Histogram* nosniff_empty_mime_type(NULL);
    if (!nosniff_empty_mime_type)
      nosniff_empty_mime_type = base::BooleanHistogram::FactoryGet(
          "nosniff.empty_mime_type",
          base::Histogram::kUmaTargetedHistogramFlag);
    nosniff_empty_mime_type->AddBoolean(mime_type.empty());
  }
}

}  // namespace

BufferedResourceHandler::BufferedResourceHandler(
    ResourceHandler* handler,
    ResourceDispatcherHostImpl* host,
    net::URLRequest* request)
    : LayeredResourceHandler(handler),
      host_(host),
      request_(request),
      read_buffer_size_(0),
      bytes_read_(0),
      sniff_content_(false),
      wait_for_plugins_(false),
      buffering_(false),
      next_handler_needs_response_started_(false),
      next_handler_needs_will_read_(false),
      finished_(false) {
}

bool BufferedResourceHandler::OnResponseStarted(
    int request_id,
    ResourceResponse* response) {
  response_ = response;
  if (!DelayResponse())
    return CompleteResponseStarted(request_id);
  return true;
}

void BufferedResourceHandler::OnRequestClosed() {
  request_ = NULL;
  next_handler_->OnRequestClosed();
}

// We'll let the original event handler provide a buffer, and reuse it for
// subsequent reads until we're done buffering.
bool BufferedResourceHandler::OnWillRead(int request_id, net::IOBuffer** buf,
                                         int* buf_size, int min_size) {
  if (buffering_) {
    DCHECK(!my_buffer_.get());
    my_buffer_ = new net::IOBuffer(net::kMaxBytesToSniff);
    *buf = my_buffer_.get();
    *buf_size = net::kMaxBytesToSniff;
    return true;
  }

  if (finished_)
    return false;

  if (!next_handler_->OnWillRead(request_id, buf, buf_size, min_size)) {
    return false;
  }
  read_buffer_ = *buf;
  read_buffer_size_ = *buf_size;
  DCHECK_GE(read_buffer_size_, net::kMaxBytesToSniff * 2);
  bytes_read_ = 0;
  return true;
}

bool BufferedResourceHandler::OnReadCompleted(int request_id, int* bytes_read) {
  ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(request_);

  if (sniff_content_) {
    if (KeepBuffering(*bytes_read))
      return true;

    *bytes_read = bytes_read_;

    // Done buffering, send the pending ResponseStarted event.
    if (!CompleteResponseStarted(request_id))
      return false;

    // The next handler might have paused the request in OnResponseStarted.
    if (info->pause_count())
      return true;
  } else if (wait_for_plugins_) {
    return true;
  }

  if (!ForwardPendingEventsToNextHandler(request_id))
    return false;
  if (info->pause_count())
    return true;

  // Release the reference that we acquired at OnWillRead.
  read_buffer_ = NULL;
  return next_handler_->OnReadCompleted(request_id, bytes_read);
}

BufferedResourceHandler::~BufferedResourceHandler() {}

bool BufferedResourceHandler::DelayResponse() {
  std::string mime_type;
  request_->GetMimeType(&mime_type);

  std::string content_type_options;
  request_->GetResponseHeaderByName("x-content-type-options",
                                    &content_type_options);

  const bool sniffing_blocked =
      LowerCaseEqualsASCII(content_type_options, "nosniff");
  const bool not_modified_status =
      response_->headers && response_->headers->response_code() == 304;
  const bool we_would_like_to_sniff = not_modified_status ?
      false : net::ShouldSniffMimeType(request_->url(), mime_type);

  RecordSnifferMetrics(sniffing_blocked, we_would_like_to_sniff, mime_type);

  if (!sniffing_blocked && we_would_like_to_sniff) {
    // We're going to look at the data before deciding what the content type
    // is.  That means we need to delay sending the ResponseStarted message
    // over the IPC channel.
    sniff_content_ = true;
    VLOG(1) << "To buffer: " << request_->url().spec();
    return true;
  }

  if (sniffing_blocked && mime_type.empty() && !not_modified_status) {
    // Ugg.  The server told us not to sniff the content but didn't give us a
    // mime type.  What's a browser to do?  Turns out, we're supposed to treat
    // the response as "text/plain".  This is the most secure option.
    mime_type.assign("text/plain");
    response_->mime_type.assign(mime_type);
  }

  if (!not_modified_status && ShouldWaitForPlugins()) {
    wait_for_plugins_ = true;
    return true;
  }

  return false;
}

bool BufferedResourceHandler::DidBufferEnough(int bytes_read) {
  const int kRequiredLength = 256;

  return bytes_read >= kRequiredLength;
}

bool BufferedResourceHandler::KeepBuffering(int bytes_read) {
  DCHECK(read_buffer_);
  if (my_buffer_) {
    // We are using our own buffer to read, update the main buffer.
    // TODO(darin): We should handle the case where read_buffer_size_ is small!
    // See RedirectToFileResourceHandler::BufIsFull to see how this impairs
    // downstream ResourceHandler implementations.
    CHECK_LT(bytes_read + bytes_read_, read_buffer_size_);
    memcpy(read_buffer_->data() + bytes_read_, my_buffer_->data(), bytes_read);
    my_buffer_ = NULL;
  }
  bytes_read_ += bytes_read;
  finished_ = (bytes_read == 0);

  if (sniff_content_) {
    std::string type_hint, new_type;
    request_->GetMimeType(&type_hint);

    if (!net::SniffMimeType(read_buffer_->data(), bytes_read_,
                            request_->url(), type_hint, &new_type)) {
      // SniffMimeType() returns false if there is not enough data to determine
      // the mime type. However, even if it returns false, it returns a new type
      // that is probably better than the current one.
      DCHECK_LT(bytes_read_, net::kMaxBytesToSniff);
      if (!finished_) {
        buffering_ = true;
        return true;
      }
    }
    sniff_content_ = false;
    response_->mime_type.assign(new_type);

    // We just sniffed the mime type, maybe there is a doctype to process.
    if (ShouldWaitForPlugins())
      wait_for_plugins_ = true;
  }

  buffering_ = false;

  if (wait_for_plugins_)
    return true;

  return false;
}

bool BufferedResourceHandler::CompleteResponseStarted(int request_id) {
  ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(request_);
  std::string mime_type;
  request_->GetMimeType(&mime_type);

  // Check if this is an X.509 certificate, if yes, let it be handled
  // by X509UserCertResourceHandler.
  if (mime_type == "application/x-x509-user-cert") {
    // This is entirely similar to how DownloadResourceThrottle works except we
    // are doing it for an X.509 client certificates.
    // TODO(darin): This does not belong here!

    if (response_->headers &&  // Can be NULL if FTP.
        response_->headers->response_code() / 100 != 2) {
      // The response code indicates that this is an error page, but we are
      // expecting an X.509 user certificate. We follow Firefox here and show
      // our own error page instead of handling the error page as a
      // certificate.
      // TODO(abarth): We should abstract the response_code test, but this kind
      //               of check is scattered throughout our codebase.
      request_->CancelWithError(net::ERR_FILE_NOT_FOUND);
      return false;
    }

    X509UserCertResourceHandler* x509_cert_handler =
        new X509UserCertResourceHandler(request_,
                                        info->GetChildID(),
                                        info->GetRouteID());
    if (!UseAlternateResourceHandler(request_id, x509_cert_handler))
      return false;
  }

  // Check to see if we should forward the data from this request to the
  // download thread.
  // TODO(paulg): Only download if the context from the renderer allows it.
  if (info->allow_download() && ShouldDownload(NULL)) {
    if (response_->headers &&  // Can be NULL if FTP.
        response_->headers->response_code() / 100 != 2) {
      // The response code indicates that this is an error page, but we don't
      // know how to display the content.  We follow Firefox here and show our
      // own error page instead of triggering a download.
      // TODO(abarth): We should abstract the response_code test, but this kind
      //               of check is scattered throughout our codebase.
      request_->CancelWithError(net::ERR_FILE_NOT_FOUND);
      return false;
    }

    info->set_is_download(true);

    scoped_refptr<ResourceHandler> handler(
        host_->CreateResourceHandlerForDownload(
            request_,
            info->GetContext(),
            info->GetChildID(),
            info->GetRouteID(),
            info->GetRequestID(),
            true,  // is_content_initiated
            DownloadSaveInfo(),
            DownloadResourceHandler::OnStartedCallback()));

    if (!UseAlternateResourceHandler(request_id, handler))
      return false;
  }

  if (info->pause_count())
    return true;

  return next_handler_->OnResponseStarted(request_id, response_);
}

bool BufferedResourceHandler::ShouldWaitForPlugins() {
  bool need_plugin_list;
  if (!ShouldDownload(&need_plugin_list) || !need_plugin_list)
    return false;

  // We don't want to keep buffering as our buffer will fill up.
  ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(request_);
  host_->PauseRequest(info->GetChildID(), info->GetRequestID(), true);

  // Get the plugins asynchronously.
  PluginServiceImpl::GetInstance()->GetPlugins(
      base::Bind(&BufferedResourceHandler::OnPluginsLoaded, this));
  return true;
}

// This test mirrors the decision that WebKit makes in
// WebFrameLoaderClient::dispatchDecidePolicyForMIMEType.
bool BufferedResourceHandler::ShouldDownload(bool* need_plugin_list) {
  if (need_plugin_list)
    *need_plugin_list = false;
  std::string type = StringToLowerASCII(response_->mime_type);

  // First, examine Content-Disposition.
  std::string disposition;
  request_->GetResponseHeaderByName("content-disposition", &disposition);
  if (!disposition.empty()) {
    net::HttpContentDisposition parsed_disposition(disposition, std::string());
    if (parsed_disposition.is_attachment())
      return true;
  }

  if (host_->delegate() &&
      host_->delegate()->ShouldForceDownloadResource(request_->url(), type))
    return true;

  // MIME type checking.
  if (net::IsSupportedMimeType(type))
    return false;

  // Finally, check the plugin list.
  bool allow_wildcard = false;
  ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(request_);
  bool stale = false;
  webkit::WebPluginInfo plugin;
  bool found = PluginServiceImpl::GetInstance()->GetPluginInfo(
      info->GetChildID(), info->GetRouteID(), info->GetContext(),
      request_->url(), GURL(), type, allow_wildcard,
      &stale, &plugin, NULL);

  if (need_plugin_list) {
    if (stale) {
      *need_plugin_list = true;
      return true;
    }
  } else {
    DCHECK(!stale);
  }

  return !found;
}

bool BufferedResourceHandler::UseAlternateResourceHandler(
    int request_id,
    ResourceHandler* handler) {
  // Inform the original ResourceHandler that this will be handled entirely by
  // the new ResourceHandler.
  // TODO(darin): We should probably check the return values of these.
  next_handler_->OnResponseStarted(request_id, response_);
  net::URLRequestStatus status(net::URLRequestStatus::HANDLED_EXTERNALLY, 0);
  next_handler_->OnResponseCompleted(request_id, status, std::string());

  // Remove the non-owning pointer to the CrossSiteResourceHandler, if any,
  // from the extra request info because the CrossSiteResourceHandler (part of
  // the original ResourceHandler chain) will be deleted by the next statement.
  ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(request_);
  info->set_cross_site_handler(NULL);

  // This is handled entirely within the new ResourceHandler, so just reset the
  // original ResourceHandler.
  next_handler_ = handler;

  next_handler_needs_response_started_ = true;
  next_handler_needs_will_read_ = true;

  return ForwardPendingEventsToNextHandler(request_id);
}

bool BufferedResourceHandler::ForwardPendingEventsToNextHandler(
    int request_id) {
  ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(request_);
  if (info->pause_count())
    return true;

  if (next_handler_needs_response_started_) {
    if (!next_handler_->OnResponseStarted(request_id, response_))
      return false;
    // If the request was paused during OnResponseStarted, we need to avoid
    // calling OnResponseStarted again.
    next_handler_needs_response_started_ = false;
    if (info->pause_count())
      return true;
  }

  if (next_handler_needs_will_read_) {
    CopyReadBufferToNextHandler(request_id);
    // If the request was paused during OnWillRead, we need to be sure to try
    // calling OnWillRead again.
    if (info->pause_count())
      return true;
    next_handler_needs_will_read_ = false;
  }
  return true;
}

void BufferedResourceHandler::CopyReadBufferToNextHandler(int request_id) {
  if (!bytes_read_)
    return;

  net::IOBuffer* buf = NULL;
  int buf_len = 0;
  if (next_handler_->OnWillRead(request_id, &buf, &buf_len, bytes_read_)) {
    CHECK((buf_len >= bytes_read_) && (bytes_read_ >= 0));
    memcpy(buf->data(), read_buffer_->data(), bytes_read_);
  }
}

void BufferedResourceHandler::OnPluginsLoaded(
    const std::vector<webkit::WebPluginInfo>& plugins) {
  wait_for_plugins_ = false;
  if (!request_)
    return;

  ResourceRequestInfoImpl* info =
      ResourceRequestInfoImpl::ForRequest(request_);
  int child_id = info->GetChildID();
  int request_id = info->GetRequestID();

  host_->PauseRequest(child_id, request_id, false);
  if (!CompleteResponseStarted(request_id))
    host_->CancelRequest(child_id, request_id, false);
}

}  // namespace content
