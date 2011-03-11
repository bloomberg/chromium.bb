// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/devtools_netlog_observer.h"

#include "base/string_util.h"
#include "chrome/browser/io_thread.h"
#include "content/common/resource_response.h"
#include "net/base/load_flags.h"
#include "net/http/http_net_log_params.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_netlog_params.h"
#include "webkit/glue/resource_loader_bridge.h"

const size_t kMaxNumEntries = 1000;

DevToolsNetLogObserver* DevToolsNetLogObserver::instance_ = NULL;

DevToolsNetLogObserver::DevToolsNetLogObserver(ChromeNetLog* chrome_net_log)
    : ChromeNetLog::ThreadSafeObserver(net::NetLog::LOG_ALL_BUT_BYTES),
      chrome_net_log_(chrome_net_log) {
  chrome_net_log_->AddObserver(this);
}

DevToolsNetLogObserver::~DevToolsNetLogObserver() {
  chrome_net_log_->RemoveObserver(this);
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

  if (type == net::NetLog::TYPE_URL_REQUEST_START_JOB) {
    if (phase != net::NetLog::PHASE_BEGIN)
      return;
    int load_flags = static_cast<net::URLRequestStartEventParameters*>(params)->
        load_flags();
    if (!(load_flags & net::LOAD_REPORT_RAW_HEADERS))
      return;
    if (request_to_info_.size() > kMaxNumEntries) {
      LOG(WARNING) << "The raw headers observer url request count has grown "
                      "larger than expected, resetting";
      request_to_info_.clear();
    }
    scoped_refptr<ResourceInfo> new_record(new ResourceInfo());
    // We may encounter multiple PHASE_BEGIN for same resource in case of
    // redirect -- if so, replace the old record to avoid keeping headers
    // from different requests.
    std::pair<RequestToInfoMap::iterator, bool> inserted =
        request_to_info_.insert(std::make_pair(source.id, new_record));
    if (!inserted.second)
      inserted.first->second = new_record;
    return;
  }
  if (type == net::NetLog::TYPE_REQUEST_ALIVE &&
      phase == net::NetLog::PHASE_END) {
    request_to_info_.erase(source.id);
    return;
  }
  if (type != net::NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST_HEADERS &&
      type != net::NetLog::TYPE_HTTP_TRANSACTION_READ_RESPONSE_HEADERS)
    return;

  ResourceInfo* info = GetResourceInfo(source.id);
  if (!info)
    return;

  switch (type) {
    case net::NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST_HEADERS: {
      const net::HttpRequestHeaders &request_headers =
          static_cast<net::NetLogHttpRequestParameter*>(params)->GetHeaders();
      for (net::HttpRequestHeaders::Iterator it(request_headers);
           it.GetNext();) {
        info->request_headers.push_back(std::make_pair(it.name(),
                                                       it.value()));
      }
      break;
    }
    case net::NetLog::TYPE_HTTP_TRANSACTION_READ_RESPONSE_HEADERS: {
      const net::HttpResponseHeaders& response_headers =
          static_cast<net::NetLogHttpResponseParameter*>(params)->GetHeaders();
      info->http_status_code = response_headers.response_code();
      info->http_status_text = response_headers.GetStatusText();
      std::string name, value;
      for (void* it = NULL;
           response_headers.EnumerateHeaderLines(&it, &name, &value); ) {
        info->response_headers.push_back(std::make_pair(name, value));
      }
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void DevToolsNetLogObserver::Attach(IOThread* io_thread) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!instance_);

  instance_ = new DevToolsNetLogObserver(io_thread->net_log());
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
void DevToolsNetLogObserver::PopulateResponseInfo(net::URLRequest* request,
                                                  ResourceResponse* response) {
  if (!(request->load_flags() & net::LOAD_REPORT_RAW_HEADERS))
    return;

  uint32 source_id = request->net_log().source().id;
  DevToolsNetLogObserver* dev_tools_net_log_observer =
      DevToolsNetLogObserver::GetInstance();
  if (!dev_tools_net_log_observer)
    return;
  response->response_head.devtools_info =
      dev_tools_net_log_observer->GetResourceInfo(source_id);
}
