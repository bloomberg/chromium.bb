// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/network_handler.h"

#include <stddef.h>

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

void GotCookiesOnIO(
    const net::CookieStore::GetCookieListCallback& callback,
    const net::CookieList& cookie_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(callback, cookie_list));
}

void GetCookiesForURLOnIO(
    ResourceContext* resource_context,
    net::URLRequestContextGetter* context_getter,
    const GURL& url,
    const net::CookieStore::GetCookieListCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::URLRequestContext* request_context =
      GetRequestContextOnIO(resource_context, context_getter, url);
  request_context->cookie_store()->GetAllCookiesForURLAsync(
      url, base::Bind(&GotCookiesOnIO, callback));
}

void GetAllCookiesOnIO(
    ResourceContext* resource_context,
    net::URLRequestContextGetter* context_getter,
    const net::CookieStore::GetCookieListCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::URLRequestContext* request_context =
      context_getter->GetURLRequestContext();
  request_context->cookie_store()->GetAllCookiesAsync(
      base::Bind(&GotCookiesOnIO, callback));
}

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

template <typename Callback>
class GetCookiesCommandBase {
 public:
  GetCookiesCommandBase(std::unique_ptr<Callback> callback)
      : callback_(std::move(callback)), request_count_(0) {}

 protected:
  void GotCookiesForURL(const net::CookieList& cookie_list) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    for (const net::CanonicalCookie& cookie : cookie_list) {
      std::string key = base::StringPrintf(
          "%s::%s::%s::%d", cookie.Name().c_str(), cookie.Domain().c_str(),
          cookie.Path().c_str(), cookie.IsSecure());
      cookies_[key] = cookie;
    }
    --request_count_;
    if (!request_count_) {
      SendResponse();
      delete this;
    }
  }

  void SendResponse() {
    std::unique_ptr<protocol::Array<Network::Cookie>> cookies =
        protocol::Array<Network::Cookie>::create();
    for (const auto& pair : cookies_) {
      const net::CanonicalCookie& cookie = pair.second;
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
    callback_->sendSuccess(std::move(cookies));
  }

  std::unique_ptr<Callback> callback_;
  int request_count_;
  base::hash_map<std::string, net::CanonicalCookie> cookies_;
};

class GetCookiesCommand : public GetCookiesCommandBase<GetCookiesCallback> {
 public:
  GetCookiesCommand(RenderFrameHostImpl* frame_host,
                    std::unique_ptr<GetCookiesCallback> callback)
      : GetCookiesCommandBase(std::move(callback)) {
    net::CookieStore::GetCookieListCallback got_cookies_callback = base::Bind(
        &GetCookiesCommand::GotCookiesForURL, base::Unretained(this));

    std::queue<FrameTreeNode*> queue;
    queue.push(frame_host->frame_tree_node());
    while (!queue.empty()) {
      FrameTreeNode* node = queue.front();
      queue.pop();

      // Only traverse nodes with the same local root.
      if (node->current_frame_host()->IsCrossProcessSubframe())
        continue;
      ++request_count_;
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&GetCookiesForURLOnIO,
                     base::Unretained(frame_host->GetSiteInstance()
                                          ->GetBrowserContext()
                                          ->GetResourceContext()),
                     base::Unretained(frame_host->GetProcess()
                                          ->GetStoragePartition()
                                          ->GetURLRequestContext()),
                     node->current_url(), got_cookies_callback));

      for (size_t i = 0; i < node->child_count(); ++i)
        queue.push(node->child_at(i));
    }
  }
};

class GetAllCookiesCommand
    : public GetCookiesCommandBase<GetAllCookiesCallback> {
 public:
  GetAllCookiesCommand(RenderFrameHostImpl* frame_host,
                       std::unique_ptr<GetAllCookiesCallback> callback)
      : GetCookiesCommandBase(std::move(callback)) {
    net::CookieStore::GetCookieListCallback got_cookies_callback = base::Bind(
        &GetAllCookiesCommand::GotCookiesForURL, base::Unretained(this));

    request_count_ = 1;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&GetAllCookiesOnIO,
                   base::Unretained(frame_host->GetSiteInstance()
                                        ->GetBrowserContext()
                                        ->GetResourceContext()),
                   base::Unretained(frame_host->GetProcess()
                                        ->GetStoragePartition()
                                        ->GetURLRequestContext()),
                   got_cookies_callback));
  }
};

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
    std::unique_ptr<GetCookiesCallback> callback) {
  if (!host_)
    callback->sendFailure(Response::InternalError());
  else
    new GetCookiesCommand(host_, std::move(callback));
}

void NetworkHandler::GetAllCookies(
    std::unique_ptr<GetAllCookiesCallback> callback) {
  if (!host_)
    callback->sendFailure(Response::InternalError());
  else
    new GetAllCookiesCommand(host_, std::move(callback));
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
