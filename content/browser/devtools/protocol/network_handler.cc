// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/network_handler.h"

#include <stddef.h>

#include "base/barrier_closure.h"
#include "base/containers/hash_tables.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace content {
namespace protocol {
namespace {

using ProtocolCookieArray = protocol::Array<Network::Cookie>;
using GetCookiesCallback = protocol::Network::Backend::GetCookiesCallback;
using GetAllCookiesCallback = protocol::Network::Backend::GetAllCookiesCallback;
using SetCookieCallback = protocol::Network::Backend::SetCookieCallback;
using DeleteCookieCallback = protocol::Network::Backend::DeleteCookieCallback;

net::URLRequestContext* GetRequestContextOnIO(
    ResourceContext* resource_context,
    net::URLRequestContextGetter* context_getter,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::URLRequestContext* context =
      GetContentClient()->browser()->OverrideRequestContextForURL(
          url, resource_context);
  if (!context)
    context = context_getter->GetURLRequestContext();
  return context;
}

class CookieRetriever : public base::RefCountedThreadSafe<CookieRetriever> {
  public:
    CookieRetriever(std::unique_ptr<GetCookiesCallback> callback)
        : callback_(std::move(callback)),
          all_callback_(nullptr) {}

    CookieRetriever(std::unique_ptr<GetAllCookiesCallback> callback)
        : callback_(nullptr),
          all_callback_(std::move(callback)) {}

    void RetrieveCookiesOnIO(
        ResourceContext* resource_context,
        net::URLRequestContextGetter* context_getter,
        const std::vector<GURL>& urls) {
      DCHECK_CURRENTLY_ON(BrowserThread::IO);
      callback_count_ = urls.size();

      if (callback_count_ == 0) {
        GotAllCookies();
        return;
      }

      for (const GURL& url : urls) {
        net::URLRequestContext* request_context =
            GetRequestContextOnIO(resource_context, context_getter, url);
        request_context->cookie_store()->GetAllCookiesForURLAsync(url,
            base::Bind(&CookieRetriever::GotCookies, this));
      }
    }

    void RetrieveAllCookiesOnIO(
        ResourceContext* resource_context,
        net::URLRequestContextGetter* context_getter) {
      DCHECK_CURRENTLY_ON(BrowserThread::IO);
      callback_count_ = 1;

      net::URLRequestContext* request_context =
          context_getter->GetURLRequestContext();
      request_context->cookie_store()->GetAllCookiesAsync(
          base::Bind(&CookieRetriever::GotCookies, this));
    }
  protected:
    virtual ~CookieRetriever() {}

    void GotCookies(const net::CookieList& cookie_list) {
      DCHECK_CURRENTLY_ON(BrowserThread::IO);
      for (const net::CanonicalCookie& cookie : cookie_list) {
        std::string key = base::StringPrintf(
            "%s::%s::%s::%d", cookie.Name().c_str(), cookie.Domain().c_str(),
            cookie.Path().c_str(), cookie.IsSecure());
        cookies_[key] = cookie;
      }

      --callback_count_;
      if (callback_count_ == 0)
        GotAllCookies();
    }

    void GotAllCookies() {
      net::CookieList master_cookie_list;
      for (const auto& pair : cookies_)
        master_cookie_list.push_back(pair.second);

      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&CookieRetriever::SendCookiesResponseOnUI,
                     this,
                     master_cookie_list));
    }

    void SendCookiesResponseOnUI(const net::CookieList& cookie_list) {
      DCHECK_CURRENTLY_ON(BrowserThread::UI);
      std::unique_ptr<ProtocolCookieArray> cookies =
          ProtocolCookieArray::create();

      for (const net::CanonicalCookie& cookie : cookie_list) {
       std::unique_ptr<Network::Cookie> devtools_cookie =
            Network::Cookie::Create()
            .SetName(cookie.Name())
            .SetValue(cookie.Value())
            .SetDomain(cookie.Domain())
            .SetPath(cookie.Path())
            .SetExpires(cookie.ExpiryDate().ToDoubleT() * 1000)
            .SetSize(cookie.Name().length() + cookie.Value().length())
            .SetHttpOnly(cookie.IsHttpOnly())
            .SetSecure(cookie.IsSecure())
            .SetSession(!cookie.IsPersistent())
            .Build();

       switch (cookie.SameSite()) {
         case net::CookieSameSite::STRICT_MODE:
           devtools_cookie->SetSameSite(Network::CookieSameSiteEnum::Strict);
           break;
         case net::CookieSameSite::LAX_MODE:
           devtools_cookie->SetSameSite(Network::CookieSameSiteEnum::Lax);
           break;
         case net::CookieSameSite::NO_RESTRICTION:
           break;
       }

       cookies->addItem(std::move(devtools_cookie));
      }

      if (callback_) {
        callback_->sendSuccess(std::move(cookies));
      } else {
        DCHECK(all_callback_);
        all_callback_->sendSuccess(std::move(cookies));
      }
    }

    std::unique_ptr<GetCookiesCallback> callback_;
    std::unique_ptr<GetAllCookiesCallback> all_callback_;
    int callback_count_ = 0;
    std::unordered_map<std::string, net::CanonicalCookie> cookies_;

  private:
    friend class base::RefCountedThreadSafe<CookieRetriever>;
};

void DeletedCookieOnIO(std::unique_ptr<DeleteCookieCallback> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&DeleteCookieCallback::sendSuccess,
                 base::Passed(std::move(callback))));
}

void DeleteCookieOnIO(
    ResourceContext* resource_context,
    net::URLRequestContextGetter* context_getter,
    const GURL& url,
    const std::string& cookie_name,
    std::unique_ptr<DeleteCookieCallback> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::URLRequestContext* request_context =
      GetRequestContextOnIO(resource_context, context_getter, url);
  request_context->cookie_store()->DeleteCookieAsync(
      url, cookie_name, base::Bind(&DeletedCookieOnIO,
                                   base::Passed(std::move(callback))));
}

void CookieSetOnIO(std::unique_ptr<SetCookieCallback> callback, bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&SetCookieCallback::sendSuccess,
                 base::Passed(std::move(callback)),
                 success));
}

void SetCookieOnIO(
    ResourceContext* resource_context,
    net::URLRequestContextGetter* context_getter,
    const GURL& url,
    const std::string& name,
    const std::string& value,
    const std::string& domain,
    const std::string& path,
    bool secure,
    bool http_only,
    net::CookieSameSite same_site,
    base::Time expires,
    std::unique_ptr<SetCookieCallback> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::URLRequestContext* request_context =
      GetRequestContextOnIO(resource_context, context_getter, url);

  bool are_experimental_cookie_features_enabled =
      request_context->network_delegate()
        ->AreExperimentalCookieFeaturesEnabled();

  request_context->cookie_store()->SetCookieWithDetailsAsync(
      url, name, value, domain, path,
      base::Time(),
      expires,
      base::Time(),
      secure,
      http_only,
      same_site,
      are_experimental_cookie_features_enabled,
      net::COOKIE_PRIORITY_DEFAULT,
      base::Bind(&CookieSetOnIO, base::Passed(std::move(callback))));
}

std::vector<GURL> ComputeCookieURLs(
    RenderFrameHostImpl* frame_host,
    Maybe<protocol::Array<String>>& protocol_urls) {
  std::vector<GURL> urls;

  if (protocol_urls.isJust()) {
    std::unique_ptr<protocol::Array<std::string>> actual_urls =
        protocol_urls.takeJust();

    for (size_t i = 0; i < actual_urls->length(); i++)
      urls.push_back(GURL(actual_urls->get(i)));
  } else {
    std::queue<FrameTreeNode*> queue;
    queue.push(frame_host->frame_tree_node());
    while (!queue.empty()) {
      FrameTreeNode* node = queue.front();
      queue.pop();

      urls.push_back(node->current_url());
      for (size_t i = 0; i < node->child_count(); ++i)
        queue.push(node->child_at(i));
    }
  }

  return urls;
}

}  // namespace

NetworkHandler::NetworkHandler()
    : DevToolsDomainHandler(Network::Metainfo::domainName),
      host_(nullptr),
      enabled_(false) {
}

NetworkHandler::~NetworkHandler() {
}

// static
NetworkHandler* NetworkHandler::FromSession(DevToolsSession* session) {
  return static_cast<NetworkHandler*>(
      session->GetHandlerByName(Network::Metainfo::domainName));
}

void NetworkHandler::Wire(UberDispatcher* dispatcher) {
  Network::Dispatcher::wire(dispatcher, this);
}

void NetworkHandler::SetRenderFrameHost(RenderFrameHostImpl* host) {
  host_ = host;
}

Response NetworkHandler::Enable(Maybe<int> max_total_size,
                                Maybe<int> max_resource_size) {
  enabled_ = true;
  return Response::FallThrough();
}

Response NetworkHandler::Disable() {
  enabled_ = false;
  user_agent_ = std::string();
  return Response::FallThrough();
}

Response NetworkHandler::ClearBrowserCache() {
  if (host_)
    GetContentClient()->browser()->ClearCache(host_);
  return Response::OK();
}

Response NetworkHandler::ClearBrowserCookies() {
  if (host_)
    GetContentClient()->browser()->ClearCookies(host_);
  return Response::OK();
}

void NetworkHandler::GetCookies(
    Maybe<protocol::Array<String>> protocol_urls,
    std::unique_ptr<GetCookiesCallback> callback) {
  if (!host_) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  std::vector<GURL> urls = ComputeCookieURLs(host_, protocol_urls);
  scoped_refptr<CookieRetriever> retriever =
      new CookieRetriever(std::move(callback));

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CookieRetriever::RetrieveCookiesOnIO,
                 retriever,
                 base::Unretained(host_->GetSiteInstance()
                                       ->GetBrowserContext()
                                       ->GetResourceContext()),
                 base::Unretained(host_->GetProcess()
                                       ->GetStoragePartition()
                                       ->GetURLRequestContext()),
                 urls));
}

void NetworkHandler::GetAllCookies(
    std::unique_ptr<GetAllCookiesCallback> callback) {
  if (!host_) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  scoped_refptr<CookieRetriever> retriever =
      new CookieRetriever(std::move(callback));

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CookieRetriever::RetrieveAllCookiesOnIO,
                 retriever,
                 base::Unretained(host_->GetSiteInstance()
                                       ->GetBrowserContext()
                                       ->GetResourceContext()),
                 base::Unretained(host_->GetProcess()
                                       ->GetStoragePartition()
                                       ->GetURLRequestContext())));
}

void NetworkHandler::SetCookie(
    const std::string& url,
    const std::string& name,
    const std::string& value,
    Maybe<std::string> domain,
    Maybe<std::string> path,
    Maybe<bool> secure,
    Maybe<bool> http_only,
    Maybe<std::string> same_site,
    Maybe<double> expires,
    std::unique_ptr<SetCookieCallback> callback) {
  if (!host_) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  net::CookieSameSite same_site_enum = net::CookieSameSite::DEFAULT_MODE;
  if (same_site.isJust()) {
    if (same_site.fromJust() == Network::CookieSameSiteEnum::Lax)
      same_site_enum = net::CookieSameSite::LAX_MODE;
    else if (same_site.fromJust() == Network::CookieSameSiteEnum::Strict)
      same_site_enum = net::CookieSameSite::STRICT_MODE;
  }

  base::Time expiration_date;
  if (expires.isJust()) {
    expiration_date = expires.fromJust() == 0
            ? base::Time::UnixEpoch()
            : base::Time::FromDoubleT(expires.fromJust());
  }

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &SetCookieOnIO,
      base::Unretained(host_->GetSiteInstance()->GetBrowserContext()->
                       GetResourceContext()),
      base::Unretained(host_->GetProcess()->GetStoragePartition()->
                       GetURLRequestContext()),
      GURL(url), name, value, domain.fromMaybe(""), path.fromMaybe(""),
      secure.fromMaybe(false), http_only.fromMaybe(false), same_site_enum,
      expiration_date, base::Passed(std::move(callback))));
}

void NetworkHandler::DeleteCookie(
    const std::string& cookie_name,
    const std::string& url,
    std::unique_ptr<DeleteCookieCallback> callback) {
  if (!host_) {
    callback->sendFailure(Response::InternalError());
    return;
  }
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &DeleteCookieOnIO,
      base::Unretained(host_->GetSiteInstance()->GetBrowserContext()->
                       GetResourceContext()),
      base::Unretained(host_->GetProcess()->GetStoragePartition()->
                       GetURLRequestContext()),
      GURL(url),
      cookie_name,
      base::Passed(std::move(callback))));
}

Response NetworkHandler::SetUserAgentOverride(const std::string& user_agent) {
  if (user_agent.find('\n') != std::string::npos ||
      user_agent.find('\r') != std::string::npos ||
      user_agent.find('\0') != std::string::npos) {
    return Response::InvalidParams("Invalid characters found in userAgent");
  }
  user_agent_ = user_agent;
  return Response::FallThrough();
}

Response NetworkHandler::CanEmulateNetworkConditions(bool* result) {
  *result = false;
  return Response::OK();
}

std::string NetworkHandler::UserAgentOverride() const {
  return enabled_ ? user_agent_ : std::string();
}

}  // namespace protocol
}  // namespace content
