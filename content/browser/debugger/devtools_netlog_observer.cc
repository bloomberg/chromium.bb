// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/debugger/devtools_netlog_observer.h"

#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/resource_response.h"
#include "net/base/load_flags.h"
#include "net/http/http_net_log_params.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_netlog_params.h"
#include "webkit/glue/resource_loader_bridge.h"

using content::BrowserThread;

const size_t kMaxNumEntries = 1000;

DevToolsNetLogObserver* DevToolsNetLogObserver::instance_ = NULL;

DevToolsNetLogObserver::DevToolsNetLogObserver(net::NetLog* net_log)
    : net::NetLog::ThreadSafeObserver(net::NetLog::LOG_ALL_BUT_BYTES),
      net_log_(net_log) {
  net_log_->AddThreadSafeObserver(this);
}

DevToolsNetLogObserver::~DevToolsNetLogObserver() {
  net_log_->RemoveThreadSafeObserver(this);
}

DevToolsNetLogObserver::ResourceInfo*
DevToolsNetLogObserver::GetResourceInfo(uint32 id) {
  RequestToInfoMap::iterator it = request_to_info_.find(id);
  if (it != request_to_info_.end())
    return it->second;
  return NULL;
}

void DevToolsNetLogObserver::OnAddEntry(net::NetLog::EventType type,
                                        const base::TimeTicks& time,
                                        const net::NetLog::Source& source,
                                        net::NetLog::EventPhase phase,
                                        net::NetLog::EventParameters* params) {
  // The events that the Observer is interested in only occur on the IO thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO))
    return;

  // The events that the Observer is interested in only occur on the IO thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO))
    return;
  if (source.type == net::NetLog::SOURCE_URL_REQUEST)
    OnAddURLRequestEntry(type, time, source, phase, params);
  else if (source.type == net::NetLog::SOURCE_HTTP_STREAM_JOB)
    OnAddHTTPStreamJobEntry(type, time, source, phase, params);
  else if (source.type == net::NetLog::SOURCE_SOCKET)
    OnAddSocketEntry(type, time, source, phase, params);
}

void DevToolsNetLogObserver::OnAddURLRequestEntry(
    net::NetLog::EventType type,
    const base::TimeTicks& time,
    const net::NetLog::Source& source,
    net::NetLog::EventPhase phase,
    net::NetLog::EventParameters* params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  bool is_begin = phase == net::NetLog::PHASE_BEGIN;
  bool is_end = phase == net::NetLog::PHASE_END;

  if (type == net::NetLog::TYPE_URL_REQUEST_START_JOB) {
    if (is_begin) {
      int load_flags = static_cast<
          net::URLRequestStartEventParameters*>(params)->load_flags();
      if (!(load_flags & net::LOAD_REPORT_RAW_HEADERS))
        return;

      if (request_to_info_.size() > kMaxNumEntries) {
        LOG(WARNING) << "The raw headers observer url request count has grown "
                        "larger than expected, resetting";
        request_to_info_.clear();
      }

      request_to_info_[source.id] = new ResourceInfo();

      if (request_to_encoded_data_length_.size() > kMaxNumEntries) {
        LOG(WARNING) << "The encoded data length observer url request count "
                        "has grown larger than expected, resetting";
        request_to_encoded_data_length_.clear();
      }

      request_to_encoded_data_length_[source.id] = 0;
    }
    return;
  } else if (type == net::NetLog::TYPE_REQUEST_ALIVE) {
    // Cleanup records based on the TYPE_REQUEST_ALIVE entry.
    if (is_end) {
      request_to_info_.erase(source.id);
      request_to_encoded_data_length_.erase(source.id);
    }
    return;
  }

  ResourceInfo* info = GetResourceInfo(source.id);
  if (!info)
    return;

  switch (type) {
    case net::NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST_HEADERS: {
      net::NetLogHttpRequestParameter* request_parameter =
          static_cast<net::NetLogHttpRequestParameter*>(params);
      const net::HttpRequestHeaders &request_headers =
          request_parameter->GetHeaders();

      // We need to clear headers in case the same url_request is reused for
      // several http requests (e.g. see http://crbug.com/80157).
      info->request_headers.clear();

      for (net::HttpRequestHeaders::Iterator it(request_headers);
           it.GetNext();) {
        info->request_headers.push_back(std::make_pair(it.name(),
                                                       it.value()));
      }
      info->request_headers_text =
          request_parameter->GetLine() +
          request_parameter->GetHeaders().ToString();
      break;
    }
    case net::NetLog::TYPE_HTTP_TRANSACTION_READ_RESPONSE_HEADERS: {
      const net::HttpResponseHeaders& response_headers =
          static_cast<net::NetLogHttpResponseParameter*>(params)->GetHeaders();
      info->http_status_code = response_headers.response_code();
      info->http_status_text = response_headers.GetStatusText();
      std::string name, value;

      // We need to clear headers in case the same url_request is reused for
      // several http requests (e.g. see http://crbug.com/80157).
      info->response_headers.clear();

      for (void* it = NULL;
           response_headers.EnumerateHeaderLines(&it, &name, &value); ) {
        info->response_headers.push_back(std::make_pair(name, value));
      }
      info->response_headers_text =
          net::HttpUtil::ConvertHeadersBackToHTTPResponse(
              response_headers.raw_headers());
      break;
    }
    case net::NetLog::TYPE_HTTP_STREAM_REQUEST_BOUND_TO_JOB: {
      uint32 http_stream_job_id = static_cast<net::NetLogSourceParameter*>(
          params)->value().id;
      HTTPStreamJobToSocketMap::iterator it =
          http_stream_job_to_socket_.find(http_stream_job_id);
      if (it == http_stream_job_to_socket_.end())
        return;
      uint32 socket_id = it->second;

      if (socket_to_request_.size() > kMaxNumEntries) {
        LOG(WARNING) << "The url request observer socket count has grown "
                        "larger than expected, resetting";
        socket_to_request_.clear();
      }

      socket_to_request_[socket_id] = source.id;
      http_stream_job_to_socket_.erase(http_stream_job_id);
      break;
    }
    default:
      break;
  }
}

void DevToolsNetLogObserver::OnAddHTTPStreamJobEntry(
    net::NetLog::EventType type,
    const base::TimeTicks& time,
    const net::NetLog::Source& source,
    net::NetLog::EventPhase phase,
    net::NetLog::EventParameters* params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (type == net::NetLog::TYPE_SOCKET_POOL_BOUND_TO_SOCKET) {
    uint32 socket_id = static_cast<net::NetLogSourceParameter*>(
      params)->value().id;

    // Prevents us from passively growing the memory unbounded in
    // case something went wrong. Should not happen.
    if (http_stream_job_to_socket_.size() > kMaxNumEntries) {
      LOG(WARNING) << "The load timing observer http stream job count "
                      "has grown larger than expected, resetting";
      http_stream_job_to_socket_.clear();
    }
    http_stream_job_to_socket_[source.id] = socket_id;
  }
}

void DevToolsNetLogObserver::OnAddSocketEntry(
    net::NetLog::EventType type,
    const base::TimeTicks& time,
    const net::NetLog::Source& source,
    net::NetLog::EventPhase phase,
    net::NetLog::EventParameters* params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  bool is_end = phase == net::NetLog::PHASE_END;

  SocketToRequestMap::iterator it = socket_to_request_.find(source.id);
  if (it == socket_to_request_.end())
    return;
  uint32 request_id = it->second;

  if (type == net::NetLog::TYPE_SOCKET_IN_USE) {
    if (is_end)
      socket_to_request_.erase(source.id);
    return;
  }

  RequestToEncodedDataLengthMap::iterator encoded_data_length_it =
      request_to_encoded_data_length_.find(request_id);
  if (encoded_data_length_it == request_to_encoded_data_length_.end())
    return;

  if (net::NetLog::TYPE_SOCKET_BYTES_RECEIVED == type) {
    int byte_count = 0;
    scoped_ptr<Value> value(params->ToValue());
    if (!value->IsType(Value::TYPE_DICTIONARY))
      return;

    DictionaryValue* dValue = static_cast<DictionaryValue*>(value.get());
    if (!dValue->GetInteger("byte_count", &byte_count))
      return;

    encoded_data_length_it->second += byte_count;
  }
}

void DevToolsNetLogObserver::Attach() {
  DCHECK(!instance_);

  instance_ = new DevToolsNetLogObserver(
      content::GetContentClient()->browser()->GetNetLog());
}

void DevToolsNetLogObserver::Detach() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(instance_);

  delete instance_;
  instance_ = NULL;
}

DevToolsNetLogObserver* DevToolsNetLogObserver::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  return instance_;
}

// static
void DevToolsNetLogObserver::PopulateResponseInfo(
    net::URLRequest* request,
    content::ResourceResponse* response) {
  if (!(request->load_flags() & net::LOAD_REPORT_RAW_HEADERS))
    return;

  uint32 source_id = request->net_log().source().id;
  DevToolsNetLogObserver* dev_tools_net_log_observer =
      DevToolsNetLogObserver::GetInstance();
  if (dev_tools_net_log_observer == NULL)
    return;
  response->devtools_info =
      dev_tools_net_log_observer->GetResourceInfo(source_id);
}

// static
int DevToolsNetLogObserver::GetAndResetEncodedDataLength(
    net::URLRequest* request) {
  if (!(request->load_flags() & net::LOAD_REPORT_RAW_HEADERS))
    return -1;

  uint32 source_id = request->net_log().source().id;
  DevToolsNetLogObserver* dev_tools_net_log_observer =
      DevToolsNetLogObserver::GetInstance();
  if (dev_tools_net_log_observer == NULL)
    return -1;

  RequestToEncodedDataLengthMap::iterator it =
      dev_tools_net_log_observer->request_to_encoded_data_length_.find(
          source_id);
  if (it == dev_tools_net_log_observer->request_to_encoded_data_length_.end())
    return -1;
  int encoded_data_length = it->second;
  it->second = 0;
  return encoded_data_length;
}
