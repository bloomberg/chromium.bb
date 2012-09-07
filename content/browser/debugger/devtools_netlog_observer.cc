// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/spdy/spdy_header_block.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_netlog_params.h"
#include "webkit/glue/resource_loader_bridge.h"

using content::BrowserThread;

const size_t kMaxNumEntries = 1000;

DevToolsNetLogObserver* DevToolsNetLogObserver::instance_ = NULL;

DevToolsNetLogObserver::DevToolsNetLogObserver() {
}

DevToolsNetLogObserver::~DevToolsNetLogObserver() {
}

DevToolsNetLogObserver::ResourceInfo*
DevToolsNetLogObserver::GetResourceInfo(uint32 id) {
  RequestToInfoMap::iterator it = request_to_info_.find(id);
  if (it != request_to_info_.end())
    return it->second;
  return NULL;
}

void DevToolsNetLogObserver::OnAddEntry(const net::NetLog::Entry& entry) {
  // The events that the Observer is interested in only occur on the IO thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO))
    return;

  if (entry.source().type == net::NetLog::SOURCE_URL_REQUEST)
    OnAddURLRequestEntry(entry);
  else if (entry.source().type == net::NetLog::SOURCE_HTTP_STREAM_JOB)
    OnAddHTTPStreamJobEntry(entry);
  else if (entry.source().type == net::NetLog::SOURCE_SOCKET)
    OnAddSocketEntry(entry);
}

void DevToolsNetLogObserver::OnAddURLRequestEntry(
    const net::NetLog::Entry& entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  bool is_begin = entry.phase() == net::NetLog::PHASE_BEGIN;
  bool is_end = entry.phase() == net::NetLog::PHASE_END;

  if (entry.type() == net::NetLog::TYPE_URL_REQUEST_START_JOB) {
    if (is_begin) {
      int load_flags;
      scoped_ptr<Value> event_param(entry.ParametersToValue());
      if (!net::StartEventLoadFlagsFromEventParams(event_param.get(),
                                                   &load_flags)) {
        return;
      }

      if (!(load_flags & net::LOAD_REPORT_RAW_HEADERS))
        return;

      if (request_to_info_.size() > kMaxNumEntries) {
        LOG(WARNING) << "The raw headers observer url request count has grown "
                        "larger than expected, resetting";
        request_to_info_.clear();
      }

      request_to_info_[entry.source().id] = new ResourceInfo();

      if (request_to_encoded_data_length_.size() > kMaxNumEntries) {
        LOG(WARNING) << "The encoded data length observer url request count "
                        "has grown larger than expected, resetting";
        request_to_encoded_data_length_.clear();
      }

      request_to_encoded_data_length_[entry.source().id] = 0;
    }
    return;
  } else if (entry.type() == net::NetLog::TYPE_REQUEST_ALIVE) {
    // Cleanup records based on the TYPE_REQUEST_ALIVE entry.
    if (is_end) {
      request_to_info_.erase(entry.source().id);
      request_to_encoded_data_length_.erase(entry.source().id);
    }
    return;
  }

  ResourceInfo* info = GetResourceInfo(entry.source().id);
  if (!info)
    return;

  switch (entry.type()) {
    case net::NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST_HEADERS: {
      scoped_ptr<Value> event_params(entry.ParametersToValue());
      std::string request_line;
      net::HttpRequestHeaders request_headers;

      if (!net::HttpRequestHeaders::FromNetLogParam(event_params.get(),
                                                    &request_headers,
                                                    &request_line)) {
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
    case net::NetLog::TYPE_HTTP_TRANSACTION_SPDY_SEND_REQUEST_HEADERS: {
      scoped_ptr<Value> event_params(entry.ParametersToValue());
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
        info->request_headers.push_back(std::make_pair(it->first, it->second));
      }
      info->request_headers_text = "";
      break;
    }
    case net::NetLog::TYPE_HTTP_TRANSACTION_READ_RESPONSE_HEADERS: {
      scoped_ptr<Value> event_params(entry.ParametersToValue());

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

      for (void* it = NULL;
           response_headers->EnumerateHeaderLines(&it, &name, &value); ) {
        info->response_headers.push_back(std::make_pair(name, value));
      }
      info->response_headers_text =
          net::HttpUtil::ConvertHeadersBackToHTTPResponse(
              response_headers->raw_headers());
      break;
    }
    case net::NetLog::TYPE_HTTP_STREAM_REQUEST_BOUND_TO_JOB: {
      scoped_ptr<Value> event_params(entry.ParametersToValue());
      net::NetLog::Source http_stream_job_source;
      if (!net::NetLog::Source::FromEventParameters(event_params.get(),
                                                    &http_stream_job_source)) {
        NOTREACHED();
        break;
      }

      uint32 http_stream_job_id = http_stream_job_source.id;
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

      socket_to_request_[socket_id] = entry.source().id;
      http_stream_job_to_socket_.erase(http_stream_job_id);
      break;
    }
    default:
      break;
  }
}

void DevToolsNetLogObserver::OnAddHTTPStreamJobEntry(
    const net::NetLog::Entry& entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (entry.type() == net::NetLog::TYPE_SOCKET_POOL_BOUND_TO_SOCKET) {
    scoped_ptr<Value> event_params(entry.ParametersToValue());
    net::NetLog::Source socket_source;
    if (!net::NetLog::Source::FromEventParameters(event_params.get(),
                                                  &socket_source)) {
      NOTREACHED();
      return;
    }

    // Prevents us from passively growing the memory unbounded in
    // case something went wrong. Should not happen.
    if (http_stream_job_to_socket_.size() > kMaxNumEntries) {
      LOG(WARNING) << "The load timing observer http stream job count "
                      "has grown larger than expected, resetting";
      http_stream_job_to_socket_.clear();
    }
    http_stream_job_to_socket_[entry.source().id] = socket_source.id;
  }
}

void DevToolsNetLogObserver::OnAddSocketEntry(
    const net::NetLog::Entry& entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  bool is_end = entry.phase() == net::NetLog::PHASE_END;

  SocketToRequestMap::iterator it =
      socket_to_request_.find(entry.source().id);
  if (it == socket_to_request_.end())
    return;
  uint32 request_id = it->second;

  if (entry.type() == net::NetLog::TYPE_SOCKET_IN_USE) {
    if (is_end)
      socket_to_request_.erase(entry.source().id);
    return;
  }

  RequestToEncodedDataLengthMap::iterator encoded_data_length_it =
      request_to_encoded_data_length_.find(request_id);
  if (encoded_data_length_it == request_to_encoded_data_length_.end())
    return;

  if (net::NetLog::TYPE_SOCKET_BYTES_RECEIVED == entry.type()) {
    int byte_count = 0;
    scoped_ptr<Value> value(entry.ParametersToValue());
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
  net::NetLog* net_log = content::GetContentClient()->browser()->GetNetLog();
  if (net_log) {
    instance_ = new DevToolsNetLogObserver();
    net_log->AddThreadSafeObserver(instance_, net::NetLog::LOG_ALL_BUT_BYTES);
  }
}

void DevToolsNetLogObserver::Detach() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (instance_) {
    // Safest not to do this in the destructor to maintain thread safety across
    // refactorings.
    instance_->net_log()->RemoveThreadSafeObserver(instance_);
    delete instance_;
    instance_ = NULL;
  }
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
  response->head.devtools_info =
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
