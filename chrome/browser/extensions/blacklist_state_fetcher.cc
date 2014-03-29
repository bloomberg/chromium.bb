// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/blacklist_state_fetcher.h"

#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/protocol_manager_helper.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/safe_browsing/crx_info.pb.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace {

class BlacklistRequestContextGetter : public net::URLRequestContextGetter {
 public:
  explicit BlacklistRequestContextGetter(
      net::URLRequestContextGetter* parent_context_getter) :
          network_task_runner_(
              BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    url_request_context_.reset(new net::URLRequestContext());
    url_request_context_->CopyFrom(
        parent_context_getter->GetURLRequestContext());
  }

  static void Create(
      scoped_refptr<net::URLRequestContextGetter> parent_context_getter,
      base::Callback<void(scoped_refptr<net::URLRequestContextGetter>)>
          callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    scoped_refptr<net::URLRequestContextGetter> context_getter =
        new BlacklistRequestContextGetter(parent_context_getter);
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(callback, context_getter));
  }

  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    return url_request_context_.get();
  }

  virtual scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const OVERRIDE {
    return network_task_runner_;
  }

 protected:
  virtual ~BlacklistRequestContextGetter() {
    url_request_context_->AssertNoURLRequests();
  }

 private:
  scoped_ptr<net::URLRequestContext> url_request_context_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
};

}  // namespace

namespace extensions {

BlacklistStateFetcher::BlacklistStateFetcher()
    : url_fetcher_id_(0),
      weak_ptr_factory_(this) {}

BlacklistStateFetcher::~BlacklistStateFetcher() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  STLDeleteContainerPairFirstPointers(requests_.begin(), requests_.end());
  requests_.clear();
}

void BlacklistStateFetcher::Request(const std::string& id,
                                    const RequestCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!safe_browsing_config_) {
    if (g_browser_process && g_browser_process->safe_browsing_service()) {
      SetSafeBrowsingConfig(
          g_browser_process->safe_browsing_service()->GetProtocolConfig());
    } else {
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE, base::Bind(callback, BLACKLISTED_UNKNOWN));
      return;
    }
  }

  bool request_already_sent = ContainsKey(callbacks_, id);
  callbacks_.insert(std::make_pair(id, callback));
  if (request_already_sent)
    return;

  if (url_request_context_getter_ ||
      !g_browser_process || !g_browser_process->safe_browsing_service()) {
    SendRequest(id);
  } else {
    scoped_refptr<net::URLRequestContextGetter> parent_request_context;
    if (g_browser_process && g_browser_process->safe_browsing_service()) {
      parent_request_context = g_browser_process->safe_browsing_service()
                                                ->url_request_context();
    } else {
      parent_request_context = parent_request_context_for_test_;
    }

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&BlacklistRequestContextGetter::Create,
                   parent_request_context,
                   base::Bind(&BlacklistStateFetcher::SaveRequestContext,
                              weak_ptr_factory_.GetWeakPtr(),
                              id)));
  }
}

void BlacklistStateFetcher::SaveRequestContext(
    const std::string& id,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!url_request_context_getter_)
    url_request_context_getter_ = request_context_getter;
  SendRequest(id);
}

void BlacklistStateFetcher::SendRequest(const std::string& id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ClientCRXListInfoRequest request;
  request.set_id(id);
  std::string request_str;
  request.SerializeToString(&request_str);

  GURL request_url = RequestUrl();
  net::URLFetcher* fetcher = net::URLFetcher::Create(url_fetcher_id_++,
                                                     request_url,
                                                     net::URLFetcher::POST,
                                                     this);
  requests_[fetcher] = id;
  fetcher->SetAutomaticallyRetryOn5xx(false);  // Don't retry on error.
  fetcher->SetRequestContext(url_request_context_getter_);
  fetcher->SetUploadData("application/octet-stream", request_str);
  fetcher->Start();
}

void BlacklistStateFetcher::SetSafeBrowsingConfig(
    const SafeBrowsingProtocolConfig& config) {
  safe_browsing_config_.reset(new SafeBrowsingProtocolConfig(config));
}

void BlacklistStateFetcher::SetURLRequestContextForTest(
      net::URLRequestContextGetter* parent_request_context) {
  parent_request_context_for_test_ = parent_request_context;
}

GURL BlacklistStateFetcher::RequestUrl() const {
  std::string url = base::StringPrintf(
      "%s/%s?client=%s&appver=%s&pver=2.2",
      safe_browsing_config_->url_prefix.c_str(),
      "clientreport/crx-list-info",
      safe_browsing_config_->client_name.c_str(),
      safe_browsing_config_->version.c_str());
  std::string api_key = google_apis::GetAPIKey();
  if (!api_key.empty()) {
    base::StringAppendF(&url, "&key=%s",
                        net::EscapeQueryParamValue(api_key, true).c_str());
  }
  return GURL(url);
}

void BlacklistStateFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::map<const net::URLFetcher*, std::string>::iterator it =
     requests_.find(source);
  if (it == requests_.end()) {
    NOTREACHED();
    return;
  }

  scoped_ptr<const net::URLFetcher> fetcher;

  fetcher.reset(it->first);
  std::string id = it->second;
  requests_.erase(it);

  BlacklistState state;

  if (source->GetStatus().is_success() && source->GetResponseCode() == 200) {
    std::string data;
    source->GetResponseAsString(&data);
    ClientCRXListInfoResponse response;
    if (response.ParseFromString(data)) {
      state = static_cast<BlacklistState>(response.verdict());
    } else {
      state = BLACKLISTED_UNKNOWN;
    }
  } else {
    if (source->GetStatus().status() == net::URLRequestStatus::FAILED) {
      VLOG(1) << "Blacklist request for: " << id
              << " failed with error: " << source->GetStatus().error();
    } else {
      VLOG(1) << "Blacklist request for: " << id
              << " failed with error: " << source->GetResponseCode();
    }

    state = BLACKLISTED_UNKNOWN;
  }

  std::pair<CallbackMultiMap::iterator, CallbackMultiMap::iterator> range =
      callbacks_.equal_range(id);
  for (CallbackMultiMap::const_iterator callback_it = range.first;
       callback_it != range.second;
       ++callback_it) {
    callback_it->second.Run(state);
  }

  callbacks_.erase(range.first, range.second);
}

}  // namespace extensions

