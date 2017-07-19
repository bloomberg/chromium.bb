// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/netlog_observer.h"

#include <stddef.h>

#include "base/strings/string_util.h"
#include "base/values.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/public/common/resource_response.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_entry.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/spdy/core/spdy_header_block.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_netlog_params.h"

namespace content {
const size_t kMaxNumEntries = 1000;

// static
NetLogObserver* NetLogObserver::instance_ = NULL;

// static
base::LazyInstance<std::unique_ptr<base::ThreadChecker>>::Leaky
    NetLogObserver::io_thread_checker_;

NetLogObserver::NetLogObserver() {}

NetLogObserver::~NetLogObserver() {}

NetLogObserver::ResourceInfo* NetLogObserver::GetResourceInfo(uint32_t id) {
  RequestToInfoMap::iterator it = request_to_info_.find(id);
  if (it != request_to_info_.end())
    return it->second.get();
  return NULL;
}

void NetLogObserver::OnAddEntry(const net::NetLogEntry& entry) {
  DCHECK(io_thread_checker_.Get().get());

  // The events that the Observer is interested in only occur on the IO thread.
  if (!io_thread_checker_.Get()->CalledOnValidThread())
    return;

  if (entry.source().type == net::NetLogSourceType::URL_REQUEST)
    OnAddURLRequestEntry(entry);
}

void NetLogObserver::OnAddURLRequestEntry(const net::NetLogEntry& entry) {
  bool is_begin = entry.phase() == net::NetLogEventPhase::BEGIN;
  bool is_end = entry.phase() == net::NetLogEventPhase::END;

  if (entry.type() == net::NetLogEventType::URL_REQUEST_START_JOB) {
    if (is_begin) {
      if (request_to_info_.size() > kMaxNumEntries) {
        LOG(WARNING) << "The raw headers observer url request count has grown "
                        "larger than expected, resetting";
        request_to_info_.clear();
      }

      request_to_info_[entry.source().id] = new ResourceInfo();
    }
    return;
  } else if (entry.type() == net::NetLogEventType::REQUEST_ALIVE) {
    // Cleanup records based on the TYPE_REQUEST_ALIVE entry.
    if (is_end)
      request_to_info_.erase(entry.source().id);
    return;
  }

  ResourceInfo* info = GetResourceInfo(entry.source().id);
  if (!info)
    return;

  switch (entry.type()) {
    case net::NetLogEventType::HTTP_TRANSACTION_SEND_REQUEST_HEADERS: {
      std::unique_ptr<base::Value> event_params(entry.ParametersToValue());
      std::string request_line;
      net::HttpRequestHeaders request_headers;

      if (!net::HttpRequestHeaders::FromNetLogParam(
              event_params.get(), &request_headers, &request_line)) {
        NOTREACHED();
      }

      // We need to clear headers in case the same url_request is reused for
      // several http requests (e.g. see http://crbug.com/80157).
      info->request_headers.clear();

      for (net::HttpRequestHeaders::Iterator it(request_headers);
           it.GetNext();) {
        info->request_headers.push_back(std::make_pair(it.name(), it.value()));
      }
      info->request_headers_text = request_line + request_headers.ToString();
      break;
    }
    case net::NetLogEventType::HTTP_TRANSACTION_HTTP2_SEND_REQUEST_HEADERS:
    case net::NetLogEventType::HTTP_TRANSACTION_QUIC_SEND_REQUEST_HEADERS: {
      std::unique_ptr<base::Value> event_params(entry.ParametersToValue());
      net::SpdyHeaderBlock request_headers;

      if (!net::SpdyHeaderBlockFromNetLogParam(event_params.get(),
                                               &request_headers)) {
        NOTREACHED();
      }

      // We need to clear headers in case the same url_request is reused for
      // several http requests (e.g. see http://crbug.com/80157).
      info->request_headers.clear();

      for (net::SpdyHeaderBlock::const_iterator it = request_headers.begin();
           it != request_headers.end(); ++it) {
        info->request_headers.push_back(
            std::make_pair(it->first.as_string(), it->second.as_string()));
      }
      info->request_headers_text = "";
      break;
    }
    case net::NetLogEventType::HTTP_TRANSACTION_READ_RESPONSE_HEADERS: {
      std::unique_ptr<base::Value> event_params(entry.ParametersToValue());

      scoped_refptr<net::HttpResponseHeaders> response_headers;

      if (!net::HttpResponseHeaders::FromNetLogParam(event_params.get(),
                                                     &response_headers)) {
        NOTREACHED();
      }

      info->http_status_code = response_headers->response_code();
      info->http_status_text = response_headers->GetStatusText();
      std::string name, value;

      // We need to clear headers in case the same url_request is reused for
      // several http requests (e.g. see http://crbug.com/80157).
      info->response_headers.clear();

      for (size_t it = 0;
           response_headers->EnumerateHeaderLines(&it, &name, &value);) {
        info->response_headers.push_back(std::make_pair(name, value));
      }

      if (!info->request_headers_text.empty()) {
        info->response_headers_text =
            net::HttpUtil::ConvertHeadersBackToHTTPResponse(
                response_headers->raw_headers());
      } else {
        // SPDY request.
        info->response_headers_text = "";
      }
      break;
    }
    default:
      break;
  }
}

void NetLogObserver::Attach(net::NetLog* net_log) {
  DCHECK(!instance_);
  io_thread_checker_.Get().reset(new base::ThreadChecker());
  if (net_log) {
    instance_ = new NetLogObserver();
    net_log->AddObserver(
        instance_, net::NetLogCaptureMode::IncludeCookiesAndCredentials());
  }
}

void NetLogObserver::Detach() {
  DCHECK(io_thread_checker_.Get().get() &&
         io_thread_checker_.Get()->CalledOnValidThread());
  io_thread_checker_.Get().reset();
  if (instance_) {
    // Safest not to do this in the destructor to maintain thread safety across
    // refactorings.
    instance_->net_log()->RemoveObserver(instance_);
    delete instance_;
    instance_ = NULL;
  }
}

NetLogObserver* NetLogObserver::GetInstance() {
  if (!io_thread_checker_.Get().get())
    return nullptr;

  DCHECK(io_thread_checker_.Get()->CalledOnValidThread());

  return instance_;
}

// static
void NetLogObserver::PopulateResponseInfo(net::URLRequest* request,
                                          ResourceResponse* response) {
  const ResourceRequestInfoImpl* request_info =
      ResourceRequestInfoImpl::ForRequest(request);
  if (!request_info || !request_info->ShouldReportRawHeaders())
    return;

  uint32_t source_id = request->net_log().source().id;
  NetLogObserver* dev_tools_net_log_observer = NetLogObserver::GetInstance();
  if (dev_tools_net_log_observer == NULL)
    return;
  response->head.devtools_info =
      dev_tools_net_log_observer->GetResourceInfo(source_id);
}

}  // namespace content
