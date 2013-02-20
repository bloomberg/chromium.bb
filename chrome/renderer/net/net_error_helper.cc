// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/net_error_helper.h"

#include "chrome/common/render_messages.h"
#include "chrome/common/net/net_error_info.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_view.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLError.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"

using chrome_common_net::DnsProbeResult;
using content::RenderView;
using content::RenderViewObserver;
using content::kUnreachableWebDataURL;

namespace {

GURL GetProvisionallyLoadingURLFromWebFrame(WebKit::WebFrame* frame) {
  return frame->provisionalDataSource()->request().url();
}

bool IsErrorPage(const GURL& url) {
  return (url.spec() == kUnreachableWebDataURL);
}

// Returns whether |net_error| is a DNS-related error (and therefore whether
// the tab helper should start a DNS probe after receiving it.)
bool IsDnsError(int net_error) {
  return net_error == net::ERR_NAME_NOT_RESOLVED ||
         net_error == net::ERR_NAME_RESOLUTION_FAILED;
}

NetErrorTracker::FrameType GetFrameType(WebKit::WebFrame* frame) {
  return frame->parent() ? NetErrorTracker::FRAME_SUB
                         : NetErrorTracker::FRAME_MAIN;
}

NetErrorTracker::PageType GetPageType(WebKit::WebFrame* frame) {
  bool error_page = IsErrorPage(GetProvisionallyLoadingURLFromWebFrame(frame));
  return error_page ? NetErrorTracker::PAGE_ERROR
                    : NetErrorTracker::PAGE_NORMAL;
}

NetErrorTracker::ErrorType GetErrorType(const WebKit::WebURLError& error) {
  return IsDnsError(error.reason) ? NetErrorTracker::ERROR_DNS
                                  : NetErrorTracker::ERROR_OTHER;
}

}  // namespace

NetErrorHelper::NetErrorHelper(RenderView* render_view)
    : RenderViewObserver(render_view),
      ALLOW_THIS_IN_INITIALIZER_LIST(tracker_(base::Bind(
          &NetErrorHelper::TrackerCallback,
          base::Unretained(this)))),
      dns_error_page_state_(NetErrorTracker::DNS_ERROR_PAGE_NONE),
      updated_error_page_(false) {
}

NetErrorHelper::~NetErrorHelper() {
}

void NetErrorHelper::DidStartProvisionalLoad(WebKit::WebFrame* frame) {
  tracker_.OnStartProvisionalLoad(GetFrameType(frame), GetPageType(frame));
}

void NetErrorHelper::DidFailProvisionalLoad(WebKit::WebFrame* frame,
                                            const WebKit::WebURLError& error) {
  tracker_.OnFailProvisionalLoad(GetFrameType(frame), GetErrorType(error));
}

void NetErrorHelper::DidCommitProvisionalLoad(WebKit::WebFrame* frame,
                                              bool is_new_navigation) {
  tracker_.OnCommitProvisionalLoad(GetFrameType(frame));
}

void NetErrorHelper::DidFinishLoad(WebKit::WebFrame* frame) {
  tracker_.OnFinishLoad(GetFrameType(frame));
}

bool NetErrorHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(NetErrorHelper, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_NetErrorInfo, OnNetErrorInfo)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void NetErrorHelper::OnNetErrorInfo(int dns_probe_result) {
  DVLOG(1) << "Received DNS probe result " << dns_probe_result;

  if (dns_probe_result < 0 ||
      dns_probe_result >= chrome_common_net::DNS_PROBE_MAX) {
    DLOG(WARNING) << "Ignoring DNS probe result: invalid result "
                  << dns_probe_result;
    NOTREACHED();
    return;
  }

  if (dns_error_page_state_ != NetErrorTracker::DNS_ERROR_PAGE_LOADED) {
    DVLOG(1) << "Ignoring DNS probe result: not on DNS error page.";
    return;
  }

  if (updated_error_page_) {
    DVLOG(1) << "Ignoring DNS probe result: already updated error page.";
    return;
  }

  UpdateErrorPage(static_cast<DnsProbeResult>(dns_probe_result));
  updated_error_page_ = true;
}

void NetErrorHelper::TrackerCallback(
    NetErrorTracker::DnsErrorPageState state) {
  dns_error_page_state_ = state;

  if (state == NetErrorTracker::DNS_ERROR_PAGE_LOADED)
    updated_error_page_ = false;
}

void NetErrorHelper::UpdateErrorPage(DnsProbeResult dns_probe_result) {
  DVLOG(1) << "Updating error page with result " << dns_probe_result;
  // TODO(ttuttle): Update error page.
}
