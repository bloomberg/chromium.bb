// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/net_error_helper.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/localized_error.h"
#include "chrome/common/net/net_error_info.h"
#include "chrome/common/render_messages.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "url/gurl.h"

using base::JSONWriter;
using chrome_common_net::DnsProbeStatus;
using chrome_common_net::DnsProbeStatusIsFinished;
using chrome_common_net::DnsProbeStatusToString;
using content::RenderThread;
using content::RenderView;
using content::RenderViewObserver;
using content::kUnreachableWebDataURL;

namespace {

bool IsLoadingErrorPage(WebKit::WebFrame* frame) {
  GURL url = frame->provisionalDataSource()->request().url();
  return url.spec() == kUnreachableWebDataURL;
}

bool IsMainFrame(const WebKit::WebFrame* frame) {
  return !frame->parent();
}

// Returns whether |net_error| is a DNS-related error (and therefore whether
// the tab helper should start a DNS probe after receiving it.)
bool IsDnsError(const WebKit::WebURLError& error) {
  return std::string(error.domain.utf8()) == net::kErrorDomain &&
         (error.reason == net::ERR_NAME_NOT_RESOLVED ||
          error.reason == net::ERR_NAME_RESOLUTION_FAILED);
}

}  // namespace

NetErrorHelper::NetErrorHelper(RenderView* render_view)
    : RenderViewObserver(render_view),
      last_probe_status_(chrome_common_net::DNS_PROBE_POSSIBLE),
      last_start_was_error_page_(false),
      last_fail_was_dns_error_(false),
      forwarding_probe_results_(false),
      is_failed_post_(false) {
}

NetErrorHelper::~NetErrorHelper() {
}

void NetErrorHelper::DidStartProvisionalLoad(WebKit::WebFrame* frame) {
  OnStartLoad(IsMainFrame(frame), IsLoadingErrorPage(frame));
}

void NetErrorHelper::DidFailProvisionalLoad(WebKit::WebFrame* frame,
                                            const WebKit::WebURLError& error) {
  const bool main_frame = IsMainFrame(frame);
  const bool dns_error = IsDnsError(error);

  OnFailLoad(main_frame, dns_error);

  if (main_frame && dns_error) {
    last_error_ = error;

    WebKit::WebDataSource* data_source = frame->provisionalDataSource();
    const WebKit::WebURLRequest& failed_request = data_source->request();
    is_failed_post_ = EqualsASCII(failed_request.httpMethod(), "POST");
  }
}

void NetErrorHelper::DidCommitProvisionalLoad(WebKit::WebFrame* frame,
                                              bool is_new_navigation) {
  OnCommitLoad(IsMainFrame(frame));
}

void NetErrorHelper::DidFinishLoad(WebKit::WebFrame* frame) {
  OnFinishLoad(IsMainFrame(frame));
}

void NetErrorHelper::OnStartLoad(bool is_main_frame, bool is_error_page) {
  DVLOG(1) << "OnStartLoad(is_main_frame=" << is_main_frame
           << ", is_error_page=" << is_error_page << ")";
  if (!is_main_frame)
    return;

  last_start_was_error_page_ = is_error_page;
}

void NetErrorHelper::OnFailLoad(bool is_main_frame, bool is_dns_error) {
  DVLOG(1) << "OnFailLoad(is_main_frame=" << is_main_frame
           << ", is_dns_error=" << is_dns_error << ")";

  if (!is_main_frame)
    return;

  last_fail_was_dns_error_ = is_dns_error;

  if (is_dns_error) {
    last_probe_status_ = chrome_common_net::DNS_PROBE_POSSIBLE;
    // If the helper was forwarding probe results and another DNS error has
    // occurred, stop forwarding probe results until the corresponding (new)
    // error page loads.
    forwarding_probe_results_ = false;
  }
}

void NetErrorHelper::OnCommitLoad(bool is_main_frame) {
  DVLOG(1) << "OnCommitLoad(is_main_frame=" << is_main_frame << ")";

  if (!is_main_frame)
    return;

  // Stop forwarding results.  If the page is a DNS error page, forwarding
  // will resume once the page is loaded; if not, it should stay stopped until
  // the next DNS error page.
  forwarding_probe_results_ = false;
}

void NetErrorHelper::OnFinishLoad(bool is_main_frame) {
  DVLOG(1) << "OnFinishLoad(is_main_frame=" << is_main_frame << ")";

  if (!is_main_frame)
    return;

  // If a DNS error page just finished loading, start forwarding probe results
  // to it.
  forwarding_probe_results_ =
      last_fail_was_dns_error_ && last_start_was_error_page_;

  if (forwarding_probe_results_ &&
      last_probe_status_ != chrome_common_net::DNS_PROBE_POSSIBLE) {
    DVLOG(1) << "Error page finished loading; sending saved status.";
    UpdateErrorPage();
  }
}

bool NetErrorHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(NetErrorHelper, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_NetErrorInfo, OnNetErrorInfo)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

// static
bool NetErrorHelper::GetErrorStringsForDnsProbe(
    WebKit::WebFrame* frame,
    const WebKit::WebURLError& error,
    bool is_failed_post,
    const std::string& locale,
    base::DictionaryValue* error_strings) {
  if (!IsMainFrame(frame))
    return false;

  if (!IsDnsError(error))
    return false;

  // Get the strings for a fake "DNS probe possible" error.
  WebKit::WebURLError fake_error;
  fake_error.domain = WebKit::WebString::fromUTF8(
      chrome_common_net::kDnsProbeErrorDomain);
  fake_error.reason = chrome_common_net::DNS_PROBE_POSSIBLE;
  fake_error.unreachableURL = error.unreachableURL;
  LocalizedError::GetStrings(
      fake_error, is_failed_post, locale, error_strings);
  return true;
}

void NetErrorHelper::OnNetErrorInfo(int status_num) {
  DCHECK(status_num >= 0 && status_num < chrome_common_net::DNS_PROBE_MAX);

  DVLOG(1) << "Received status " << DnsProbeStatusToString(status_num);

  DnsProbeStatus status = static_cast<DnsProbeStatus>(status_num);
  DCHECK_NE(chrome_common_net::DNS_PROBE_POSSIBLE, status);

  if (!(last_fail_was_dns_error_ || forwarding_probe_results_)) {
    DVLOG(1) << "Ignoring NetErrorInfo: no DNS error";
    return;
  }

  last_probe_status_ = status;

  if (forwarding_probe_results_)
    UpdateErrorPage();
}

void NetErrorHelper::UpdateErrorPage() {
  DCHECK(forwarding_probe_results_);

  base::DictionaryValue error_strings;
  LocalizedError::GetStrings(GetUpdatedError(),
                             is_failed_post_,
                             RenderThread::Get()->GetLocale(),
                             &error_strings);

  std::string json;
  JSONWriter::Write(&error_strings, &json);

  std::string js = "if (window.updateForDnsProbe) "
                   "updateForDnsProbe(" + json + ");";
  string16 js16;
  if (!UTF8ToUTF16(js.c_str(), js.length(), &js16)) {
    NOTREACHED();
    return;
  }

  DVLOG(1) << "Updating error page with status "
           << chrome_common_net::DnsProbeStatusToString(last_probe_status_);
  DVLOG(2) << "New strings: " << js;

  string16 frame_xpath;
  render_view()->EvaluateScript(frame_xpath, js16, 0, false);

  UMA_HISTOGRAM_ENUMERATION("DnsProbe.ErrorPageUpdateStatus",
                            last_probe_status_,
                            chrome_common_net::DNS_PROBE_MAX);
}

WebKit::WebURLError NetErrorHelper::GetUpdatedError() const {
  // If a probe didn't run or wasn't conclusive, restore the original error.
  if (last_probe_status_ == chrome_common_net::DNS_PROBE_NOT_RUN ||
      last_probe_status_ ==
          chrome_common_net::DNS_PROBE_FINISHED_INCONCLUSIVE) {
    return last_error_;
  }

  WebKit::WebURLError error;
  error.domain = WebKit::WebString::fromUTF8(
      chrome_common_net::kDnsProbeErrorDomain);
  error.reason = last_probe_status_;
  error.unreachableURL = last_error_.unreachableURL;

  return error;
}
