// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/network_handler.h"

#include <stddef.h>
#include <stdint.h>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "base/containers/queue.h"
#include "base/json/json_reader.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "content/browser/devtools/devtools_interceptor_controller.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/protocol/page.h"
#include "content/browser/devtools/protocol/security.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/common/navigation_params.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "net/base/net_errors.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/cert/ct_sct_to_string.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/http_raw_request_response_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace content {
namespace protocol {

namespace {

using ProtocolCookieArray = Array<Network::Cookie>;
using GetCookiesCallback = Network::Backend::GetCookiesCallback;
using GetAllCookiesCallback = Network::Backend::GetAllCookiesCallback;
using SetCookieCallback = Network::Backend::SetCookieCallback;
using SetCookiesCallback = Network::Backend::SetCookiesCallback;
using DeleteCookiesCallback = Network::Backend::DeleteCookiesCallback;
using ClearBrowserCookiesCallback =
    Network::Backend::ClearBrowserCookiesCallback;

const char kDevToolsEmulateNetworkConditionsClientId[] =
    "X-DevTools-Emulate-Network-Conditions-Client-Id";

class CookieRetriever : public base::RefCountedThreadSafe<CookieRetriever> {
  public:
    CookieRetriever(std::unique_ptr<GetCookiesCallback> callback)
        : callback_(std::move(callback)),
          all_callback_(nullptr) {}

    CookieRetriever(std::unique_ptr<GetAllCookiesCallback> callback)
        : callback_(nullptr),
          all_callback_(std::move(callback)) {}

    void RetrieveCookiesOnIO(
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
            context_getter->GetURLRequestContext();
        request_context->cookie_store()->GetAllCookiesForURLAsync(
            url, base::BindOnce(&CookieRetriever::GotCookies, this));
      }
    }

    void RetrieveAllCookiesOnIO(
        net::URLRequestContextGetter* context_getter) {
      DCHECK_CURRENTLY_ON(BrowserThread::IO);
      callback_count_ = 1;

      net::URLRequestContext* request_context =
          context_getter->GetURLRequestContext();
      request_context->cookie_store()->GetAllCookiesAsync(
          base::BindOnce(&CookieRetriever::GotCookies, this));
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
          BrowserThread::UI, FROM_HERE,
          base::BindOnce(&CookieRetriever::SendCookiesResponseOnUI, this,
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
                .SetExpires(cookie.ExpiryDate().is_null() ? -1 : cookie.ExpiryDate().ToDoubleT())
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

void ClearedCookiesOnIO(std::unique_ptr<ClearBrowserCookiesCallback> callback,
                        uint32_t num_deleted) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&ClearBrowserCookiesCallback::sendSuccess,
                     std::move(callback)));
}

void ClearCookiesOnIO(net::URLRequestContextGetter* context_getter,
                      std::unique_ptr<ClearBrowserCookiesCallback> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::URLRequestContext* request_context =
      context_getter->GetURLRequestContext();
  request_context->cookie_store()->DeleteAllAsync(
      base::BindOnce(&ClearedCookiesOnIO, std::move(callback)));
}

void DeletedCookiesOnIO(base::OnceClosure callback, uint32_t num_deleted) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, std::move(callback));
}

void DeleteSelectedCookiesOnIO(net::URLRequestContextGetter* context_getter,
                               const std::string& name,
                               const std::string& url_spec,
                               const std::string& domain,
                               const std::string& path,
                               base::OnceClosure callback,
                               const net::CookieList& cookie_list) {
  net::URLRequestContext* request_context =
      context_getter->GetURLRequestContext();
  std::string normalized_domain = domain;
  if (normalized_domain.empty()) {
    GURL url(url_spec);
    if (!url.SchemeIsHTTPOrHTTPS()) {
      std::move(callback).Run();
      return;
    }
    normalized_domain = url.host();
  }

  net::CookieList filtered_list;
  for (const auto& cookie : cookie_list) {
    if (cookie.Name() != name)
      continue;
    if (cookie.Domain() != normalized_domain)
      continue;
    if (!path.empty() && cookie.Path() != path)
      continue;
    filtered_list.push_back(cookie);
  }

  for (size_t i = 0; i < filtered_list.size(); ++i) {
    const auto& cookie = filtered_list[i];
    base::OnceCallback<void(uint32_t)> once_callback;
    if (i == filtered_list.size() - 1)
      once_callback = base::BindOnce(&DeletedCookiesOnIO, std::move(callback));
    request_context->cookie_store()->DeleteCanonicalCookieAsync(
        cookie, std::move(once_callback));
  }
  if (!filtered_list.size())
    std::move(callback).Run();
}

void DeleteCookiesOnIO(net::URLRequestContextGetter* context_getter,
                       const std::string& name,
                       const std::string& url,
                       const std::string& domain,
                       const std::string& path,
                       base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::URLRequestContext* request_context =
      context_getter->GetURLRequestContext();

  request_context->cookie_store()->GetAllCookiesAsync(base::BindOnce(
      &DeleteSelectedCookiesOnIO, base::Unretained(context_getter), name, url,
      domain, path, std::move(callback)));
}

void CookieSetOnIO(std::unique_ptr<SetCookieCallback> callback, bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::BindOnce(&SetCookieCallback::sendSuccess,
                                         std::move(callback), success));
}

void SetCookieOnIO(net::URLRequestContextGetter* context_getter,
                   const std::string& name,
                   const std::string& value,
                   const std::string& url_spec,
                   const std::string& domain,
                   const std::string& path,
                   bool secure,
                   bool http_only,
                   const std::string& same_site,
                   double expires,
                   base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::URLRequestContext* request_context =
      context_getter->GetURLRequestContext();

  if (url_spec.empty() && domain.empty()) {
    std::move(callback).Run(false);
    return;
  }

  std::string normalized_domain = domain;
  if (!url_spec.empty()) {
    GURL source_url = GURL(url_spec);
    if (!source_url.SchemeIsHTTPOrHTTPS()) {
      std::move(callback).Run(false);
      return;
    }

    secure = secure || source_url.SchemeIsCryptographic();
    if (normalized_domain.empty())
      normalized_domain = source_url.host();
  }

  GURL url = GURL((secure ? "https://" : "http://") + normalized_domain);
  if (!normalized_domain.empty() && normalized_domain[0] != '.')
    normalized_domain = "";

  base::Time expiration_date;
  if (expires >= 0) {
    expiration_date =
        expires ? base::Time::FromDoubleT(expires) : base::Time::UnixEpoch();
  }

  net::CookieSameSite css = net::CookieSameSite::NO_RESTRICTION;
  if (same_site == Network::CookieSameSiteEnum::Lax)
    css = net::CookieSameSite::LAX_MODE;
  if (same_site == Network::CookieSameSiteEnum::Strict)
    css = net::CookieSameSite::STRICT_MODE;

  std::unique_ptr<net::CanonicalCookie> cc(
      net::CanonicalCookie::CreateSanitizedCookie(
          url, name, value, normalized_domain, path, base::Time(),
          expiration_date, base::Time(), secure, http_only, css,
          net::COOKIE_PRIORITY_DEFAULT));
  if (!cc) {
    std::move(callback).Run(false);
    return;
  }
  request_context->cookie_store()->SetCanonicalCookieAsync(
      std::move(cc), secure, true /*modify_http_only*/, std::move(callback));
}

void CookiesSetOnIO(std::unique_ptr<SetCookiesCallback> callback,
                    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&SetCookiesCallback::sendSuccess, std::move(callback)));
}

void SetCookiesOnIO(
    net::URLRequestContextGetter* context_getter,
    std::unique_ptr<protocol::Array<Network::CookieParam>> cookies,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (size_t i = 0; i < cookies->length(); i++) {
    Network::CookieParam* cookie = cookies->get(i);

    base::OnceCallback<void(bool)> once_callback;
    if (i == cookies->length() - 1)
      once_callback = std::move(callback);

    SetCookieOnIO(context_getter, cookie->GetName(), cookie->GetValue(),
                  cookie->GetUrl(""), cookie->GetDomain(""),
                  cookie->GetPath(""), cookie->GetSecure(false),
                  cookie->GetHttpOnly(false), cookie->GetSameSite(""),
                  cookie->GetExpires(-1), std::move(once_callback));
  }
}

std::vector<GURL> ComputeCookieURLs(RenderFrameHostImpl* frame_host,
                                    Maybe<Array<String>>& protocol_urls) {
  std::vector<GURL> urls;

  if (protocol_urls.isJust()) {
    std::unique_ptr<Array<std::string>> actual_urls = protocol_urls.takeJust();

    for (size_t i = 0; i < actual_urls->length(); i++)
      urls.push_back(GURL(actual_urls->get(i)));
  } else {
    base::queue<FrameTreeNode*> queue;
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

String resourcePriority(net::RequestPriority priority) {
  switch (priority) {
    case net::MINIMUM_PRIORITY:
    case net::IDLE:
      return Network::ResourcePriorityEnum::VeryLow;
    case net::LOWEST:
      return Network::ResourcePriorityEnum::Low;
    case net::LOW:
      return Network::ResourcePriorityEnum::Medium;
    case net::MEDIUM:
      return Network::ResourcePriorityEnum::High;
    case net::HIGHEST:
      return Network::ResourcePriorityEnum::VeryHigh;
  }
  NOTREACHED();
  return Network::ResourcePriorityEnum::Medium;
}

String referrerPolicy(blink::WebReferrerPolicy referrer_policy) {
  switch (referrer_policy) {
    case blink::kWebReferrerPolicyAlways:
      return Network::Request::ReferrerPolicyEnum::UnsafeUrl;
    case blink::kWebReferrerPolicyDefault:
      if (base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kReducedReferrerGranularity)) {
        return Network::Request::ReferrerPolicyEnum::
            StrictOriginWhenCrossOrigin;
      } else {
        return Network::Request::ReferrerPolicyEnum::NoReferrerWhenDowngrade;
      }
    case blink::kWebReferrerPolicyNoReferrerWhenDowngrade:
      return Network::Request::ReferrerPolicyEnum::NoReferrerWhenDowngrade;
    case blink::kWebReferrerPolicyNever:
      return Network::Request::ReferrerPolicyEnum::NoReferrer;
    case blink::kWebReferrerPolicyOrigin:
      return Network::Request::ReferrerPolicyEnum::Origin;
    case blink::kWebReferrerPolicyOriginWhenCrossOrigin:
      return Network::Request::ReferrerPolicyEnum::OriginWhenCrossOrigin;
    case blink::kWebReferrerPolicySameOrigin:
      return Network::Request::ReferrerPolicyEnum::SameOrigin;
    case blink::kWebReferrerPolicyStrictOrigin:
      return Network::Request::ReferrerPolicyEnum::StrictOrigin;
    case blink::kWebReferrerPolicyNoReferrerWhenDowngradeOriginWhenCrossOrigin:
      return Network::Request::ReferrerPolicyEnum::StrictOriginWhenCrossOrigin;
  }
  NOTREACHED();
  return Network::Request::ReferrerPolicyEnum::NoReferrerWhenDowngrade;
}

String referrerPolicy(net::URLRequest::ReferrerPolicy referrer_policy) {
  switch (referrer_policy) {
    case net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
      return Network::Request::ReferrerPolicyEnum::NoReferrerWhenDowngrade;
    case net::URLRequest::
        REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN:
      return Network::Request::ReferrerPolicyEnum::StrictOriginWhenCrossOrigin;
    case net::URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN:
      return Network::Request::ReferrerPolicyEnum::OriginWhenCrossOrigin;
    case net::URLRequest::NEVER_CLEAR_REFERRER:
      return Network::Request::ReferrerPolicyEnum::Origin;
    case net::URLRequest::ORIGIN:
      return Network::Request::ReferrerPolicyEnum::Origin;
    case net::URLRequest::NO_REFERRER:
      return Network::Request::ReferrerPolicyEnum::NoReferrer;
    default:
      break;
  }
  NOTREACHED();
  return Network::Request::ReferrerPolicyEnum::NoReferrerWhenDowngrade;
}

String securityState(const GURL& url, const net::CertStatus& cert_status) {
  if (!url.SchemeIsCryptographic())
    return Security::SecurityStateEnum::Neutral;
  if (net::IsCertStatusError(cert_status) &&
      !net::IsCertStatusMinorError(cert_status)) {
    return Security::SecurityStateEnum::Insecure;
  }
  return Security::SecurityStateEnum::Secure;
}

DevToolsNetworkInterceptor::InterceptionStage ToInterceptorStage(
    const protocol::Network::InterceptionStage& interceptor_stage) {
  if (interceptor_stage == protocol::Network::InterceptionStageEnum::Request)
    return DevToolsNetworkInterceptor::REQUEST;
  if (interceptor_stage ==
      protocol::Network::InterceptionStageEnum::HeadersReceived)
    return DevToolsNetworkInterceptor::RESPONSE;
  NOTREACHED();
  return DevToolsNetworkInterceptor::REQUEST;
}

net::Error NetErrorFromString(const std::string& error, bool* ok) {
  *ok = true;
  if (error == Network::ErrorReasonEnum::Failed)
    return net::ERR_FAILED;
  if (error == Network::ErrorReasonEnum::Aborted)
    return net::ERR_ABORTED;
  if (error == Network::ErrorReasonEnum::TimedOut)
    return net::ERR_TIMED_OUT;
  if (error == Network::ErrorReasonEnum::AccessDenied)
    return net::ERR_ACCESS_DENIED;
  if (error == Network::ErrorReasonEnum::ConnectionClosed)
    return net::ERR_CONNECTION_CLOSED;
  if (error == Network::ErrorReasonEnum::ConnectionReset)
    return net::ERR_CONNECTION_RESET;
  if (error == Network::ErrorReasonEnum::ConnectionRefused)
    return net::ERR_CONNECTION_REFUSED;
  if (error == Network::ErrorReasonEnum::ConnectionAborted)
    return net::ERR_CONNECTION_ABORTED;
  if (error == Network::ErrorReasonEnum::ConnectionFailed)
    return net::ERR_CONNECTION_FAILED;
  if (error == Network::ErrorReasonEnum::NameNotResolved)
    return net::ERR_NAME_NOT_RESOLVED;
  if (error == Network::ErrorReasonEnum::InternetDisconnected)
    return net::ERR_INTERNET_DISCONNECTED;
  if (error == Network::ErrorReasonEnum::AddressUnreachable)
    return net::ERR_ADDRESS_UNREACHABLE;
  *ok = false;
  return net::ERR_FAILED;
}

String NetErrorToString(int net_error) {
  if (net_error == net::ERR_ABORTED)
    return Network::ErrorReasonEnum::Aborted;
  if (net_error == net::ERR_TIMED_OUT)
    return Network::ErrorReasonEnum::TimedOut;
  if (net_error == net::ERR_ACCESS_DENIED)
    return Network::ErrorReasonEnum::AccessDenied;
  if (net_error == net::ERR_CONNECTION_CLOSED)
    return Network::ErrorReasonEnum::ConnectionClosed;
  if (net_error == net::ERR_CONNECTION_RESET)
    return Network::ErrorReasonEnum::ConnectionReset;
  if (net_error == net::ERR_CONNECTION_REFUSED)
    return Network::ErrorReasonEnum::ConnectionRefused;
  if (net_error == net::ERR_CONNECTION_ABORTED)
    return Network::ErrorReasonEnum::ConnectionAborted;
  if (net_error == net::ERR_CONNECTION_FAILED)
    return Network::ErrorReasonEnum::ConnectionFailed;
  if (net_error == net::ERR_NAME_NOT_RESOLVED)
    return Network::ErrorReasonEnum::NameNotResolved;
  if (net_error == net::ERR_INTERNET_DISCONNECTED)
    return Network::ErrorReasonEnum::InternetDisconnected;
  if (net_error == net::ERR_ADDRESS_UNREACHABLE)
    return Network::ErrorReasonEnum::AddressUnreachable;
  return Network::ErrorReasonEnum::Failed;
}

bool AddInterceptedResourceType(
    const std::string& resource_type,
    base::flat_set<ResourceType>* intercepted_resource_types) {
  if (resource_type == protocol::Page::ResourceTypeEnum::Document) {
    intercepted_resource_types->insert(RESOURCE_TYPE_MAIN_FRAME);
    intercepted_resource_types->insert(RESOURCE_TYPE_SUB_FRAME);
    return true;
  }
  if (resource_type == protocol::Page::ResourceTypeEnum::Stylesheet) {
    intercepted_resource_types->insert(RESOURCE_TYPE_STYLESHEET);
    return true;
  }
  if (resource_type == protocol::Page::ResourceTypeEnum::Image) {
    intercepted_resource_types->insert(RESOURCE_TYPE_IMAGE);
    return true;
  }
  if (resource_type == protocol::Page::ResourceTypeEnum::Media) {
    intercepted_resource_types->insert(RESOURCE_TYPE_MEDIA);
    return true;
  }
  if (resource_type == protocol::Page::ResourceTypeEnum::Font) {
    intercepted_resource_types->insert(RESOURCE_TYPE_FONT_RESOURCE);
    return true;
  }
  if (resource_type == protocol::Page::ResourceTypeEnum::Script) {
    intercepted_resource_types->insert(RESOURCE_TYPE_SCRIPT);
    return true;
  }
  if (resource_type == protocol::Page::ResourceTypeEnum::XHR) {
    intercepted_resource_types->insert(RESOURCE_TYPE_XHR);
    return true;
  }
  if (resource_type == protocol::Page::ResourceTypeEnum::Fetch) {
    intercepted_resource_types->insert(RESOURCE_TYPE_PREFETCH);
    return true;
  }
  if (resource_type == protocol::Page::ResourceTypeEnum::Other) {
    intercepted_resource_types->insert(RESOURCE_TYPE_SUB_RESOURCE);
    intercepted_resource_types->insert(RESOURCE_TYPE_OBJECT);
    intercepted_resource_types->insert(RESOURCE_TYPE_WORKER);
    intercepted_resource_types->insert(RESOURCE_TYPE_SHARED_WORKER);
    intercepted_resource_types->insert(RESOURCE_TYPE_FAVICON);
    intercepted_resource_types->insert(RESOURCE_TYPE_PING);
    intercepted_resource_types->insert(RESOURCE_TYPE_SERVICE_WORKER);
    intercepted_resource_types->insert(RESOURCE_TYPE_CSP_REPORT);
    intercepted_resource_types->insert(RESOURCE_TYPE_PLUGIN_RESOURCE);
    return true;
  }
  return false;
}

double timeDelta(base::TimeTicks time,
                 base::TimeTicks start,
                 double invalid_value = -1) {
  return time.is_null() ? invalid_value : (time - start).InMillisecondsF();
}

std::unique_ptr<Network::ResourceTiming> GetTiming(
    const net::LoadTimingInfo& load_timing) {
  const base::TimeTicks kNullTicks;
  return Network::ResourceTiming::Create()
      .SetRequestTime((load_timing.request_start - kNullTicks).InSecondsF())
      .SetProxyStart(
          timeDelta(load_timing.proxy_resolve_start, load_timing.request_start))
      .SetProxyEnd(
          timeDelta(load_timing.proxy_resolve_end, load_timing.request_start))
      .SetDnsStart(timeDelta(load_timing.connect_timing.dns_start,
                             load_timing.request_start))
      .SetDnsEnd(timeDelta(load_timing.connect_timing.dns_end,
                           load_timing.request_start))
      .SetConnectStart(timeDelta(load_timing.connect_timing.connect_start,
                                 load_timing.request_start))
      .SetConnectEnd(timeDelta(load_timing.connect_timing.connect_end,
                               load_timing.request_start))
      .SetSslStart(timeDelta(load_timing.connect_timing.ssl_start,
                             load_timing.request_start))
      .SetSslEnd(timeDelta(load_timing.connect_timing.ssl_end,
                           load_timing.request_start))
      .SetWorkerStart(-1)
      .SetWorkerReady(-1)
      .SetSendStart(
          timeDelta(load_timing.send_start, load_timing.request_start))
      .SetSendEnd(timeDelta(load_timing.send_end, load_timing.request_start))
      .SetPushStart(
          timeDelta(load_timing.push_start, load_timing.request_start, 0))
      .SetPushEnd(timeDelta(load_timing.push_end, load_timing.request_start, 0))
      .SetReceiveHeadersEnd(
          timeDelta(load_timing.receive_headers_end, load_timing.request_start))
      .Build();
}

std::unique_ptr<Object> GetHeaders(const base::StringPairs& pairs) {
  std::unique_ptr<DictionaryValue> headers_dict(DictionaryValue::create());
  for (const auto& pair : pairs) {
    std::string value;
    bool merge_with_another = headers_dict->getString(pair.first, &value);
    headers_dict->setString(pair.first, merge_with_another
                                            ? value + '\n' + pair.second
                                            : pair.second);
  }
  return Object::fromValue(headers_dict.get(), nullptr);
}

String GetProtocol(const GURL& url, const network::ResourceResponseInfo& info) {
  std::string protocol = info.alpn_negotiated_protocol;
  if (protocol.empty() || protocol == "unknown") {
    if (info.was_fetched_via_spdy) {
      protocol = "spdy";
    } else if (url.SchemeIsHTTPOrHTTPS()) {
      protocol = "http";
      if (info.headers->GetHttpVersion() == net::HttpVersion(0, 9))
        protocol = "http/0.9";
      else if (info.headers->GetHttpVersion() == net::HttpVersion(1, 0))
        protocol = "http/1.0";
      else if (info.headers->GetHttpVersion() == net::HttpVersion(1, 1))
        protocol = "http/1.1";
    } else {
      protocol = url.scheme();
    }
  }
  return protocol;
}

class NetworkNavigationThrottle : public content::NavigationThrottle {
 public:
  NetworkNavigationThrottle(
      base::WeakPtr<protocol::NetworkHandler> network_handler,
      content::NavigationHandle* navigation_handle)
      : content::NavigationThrottle(navigation_handle),
        network_handler_(network_handler) {}

  ~NetworkNavigationThrottle() override {}

  // content::NavigationThrottle implementation:
  NavigationThrottle::ThrottleCheckResult WillProcessResponse() override {
    if (network_handler_ && network_handler_->ShouldCancelNavigation(
                                navigation_handle()->GetGlobalRequestID())) {
      return CANCEL_AND_IGNORE;
    }
    return PROCEED;
  }

  const char* GetNameForLogging() override {
    return "DevToolsNetworkNavigationThrottle";
  }

 private:
  base::WeakPtr<protocol::NetworkHandler> network_handler_;
  DISALLOW_COPY_AND_ASSIGN(NetworkNavigationThrottle);
};

void ConfigureServiceWorkerContextOnIO() {
  std::set<std::string> headers;
  headers.insert(kDevToolsEmulateNetworkConditionsClientId);
  content::ServiceWorkerContext::AddExcludedHeadersForFetchEvent(headers);
}

bool GetPostData(const net::URLRequest* request, std::string* post_data) {
  if (!request->has_upload())
    return false;

  const net::UploadDataStream* stream = request->get_upload();
  if (!stream->GetElementReaders())
    return false;

  const auto* element_readers = stream->GetElementReaders();

  if (element_readers->empty())
    return false;

  post_data->clear();
  for (const auto& element_reader : *element_readers) {
    const net::UploadBytesElementReader* reader =
        element_reader->AsBytesReader();
    // TODO(caseq): Also support blobs.
    if (!reader) {
      post_data->clear();
      return false;
    }
    // TODO(caseq): This should really be base64 encoded.
    post_data->append(reader->bytes(), reader->length());
  }
  return true;
}

// TODO(caseq): all problems in the above function should be fixed here as well.
bool GetPostData(const network::ResourceRequestBody& request_body,
                 std::string* result) {
  const std::vector<network::DataElement>* elements = request_body.elements();
  if (elements->empty())
    return false;
  for (const auto& element : *elements) {
    if (element.type() != network::DataElement::TYPE_BYTES)
      return false;
    result->append(element.bytes(), element.length());
  }
  return true;
}

std::string StripFragment(const GURL& url) {
  url::Replacements<char> replacements;
  replacements.ClearRef();
  return url.ReplaceComponents(replacements).spec();
}

}  // namespace

NetworkHandler::NetworkHandler(const std::string& host_id)
    : DevToolsDomainHandler(Network::Metainfo::domainName),
      browser_context_(nullptr),
      storage_partition_(nullptr),
      host_(nullptr),
      enabled_(false),
      host_id_(host_id),
      bypass_service_worker_(false),
      cache_disabled_(false),
      weak_factory_(this) {
  static bool have_configured_service_worker_context = false;
  if (have_configured_service_worker_context)
    return;
  have_configured_service_worker_context = true;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::BindOnce(&ConfigureServiceWorkerContextOnIO));
}

NetworkHandler::~NetworkHandler() {
}

// static
std::vector<NetworkHandler*> NetworkHandler::ForAgentHost(
    DevToolsAgentHostImpl* host) {
  return DevToolsSession::HandlersForAgentHost<NetworkHandler>(
      host, Network::Metainfo::domainName);
}

void NetworkHandler::Wire(UberDispatcher* dispatcher) {
  frontend_.reset(new Network::Frontend(dispatcher->channel()));
  Network::Dispatcher::wire(dispatcher, this);
}

void NetworkHandler::SetRenderer(int render_process_host_id,
                                 RenderFrameHostImpl* frame_host) {
  RenderProcessHost* process_host =
      RenderProcessHost::FromID(render_process_host_id);
  if (process_host) {
    storage_partition_ = process_host->GetStoragePartition();
    browser_context_ = process_host->GetBrowserContext();
  } else {
    storage_partition_ = nullptr;
    browser_context_ = nullptr;
  }
  host_ = frame_host;
}

Response NetworkHandler::Enable(Maybe<int> max_total_size,
                                Maybe<int> max_resource_size,
                                Maybe<int> max_post_data_size) {
  enabled_ = true;
  return Response::FallThrough();
}

Response NetworkHandler::Disable() {
  enabled_ = false;
  user_agent_ = std::string();
  interception_handle_.reset();
  SetNetworkConditions(nullptr);
  extra_headers_.clear();
  return Response::FallThrough();
}

Response NetworkHandler::SetCacheDisabled(bool cache_disabled) {
  cache_disabled_ = cache_disabled;
  return Response::FallThrough();
}

class DevtoolsClearCacheObserver
    : public content::BrowsingDataRemover::Observer {
 public:
  explicit DevtoolsClearCacheObserver(
      content::BrowsingDataRemover* remover,
      std::unique_ptr<NetworkHandler::ClearBrowserCacheCallback> callback)
      : remover_(remover), callback_(std::move(callback)) {
    remover_->AddObserver(this);
  }

  ~DevtoolsClearCacheObserver() override { remover_->RemoveObserver(this); }
  void OnBrowsingDataRemoverDone() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    callback_->sendSuccess();
    delete this;
  }

 private:
  content::BrowsingDataRemover* remover_;
  std::unique_ptr<NetworkHandler::ClearBrowserCacheCallback> callback_;
};

void NetworkHandler::ClearBrowserCache(
    std::unique_ptr<ClearBrowserCacheCallback> callback) {
  if (!browser_context_) {
    callback->sendFailure(Response::InternalError());
    return;
  }
  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(browser_context_);
  remover->RemoveAndReply(
      base::Time(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_CACHE,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
      new DevtoolsClearCacheObserver(remover, std::move(callback)));
}

void NetworkHandler::ClearBrowserCookies(
    std::unique_ptr<ClearBrowserCookiesCallback> callback) {
  if (!storage_partition_) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          &ClearCookiesOnIO,
          base::Unretained(storage_partition_->GetURLRequestContext()),
          std::move(callback)));
}

void NetworkHandler::GetCookies(Maybe<Array<String>> protocol_urls,
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
      base::BindOnce(
          &CookieRetriever::RetrieveCookiesOnIO, retriever,
          base::Unretained(storage_partition_->GetURLRequestContext()), urls));
}

void NetworkHandler::GetAllCookies(
    std::unique_ptr<GetAllCookiesCallback> callback) {
  if (!storage_partition_) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  scoped_refptr<CookieRetriever> retriever =
      new CookieRetriever(std::move(callback));

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          &CookieRetriever::RetrieveAllCookiesOnIO, retriever,
          base::Unretained(storage_partition_->GetURLRequestContext())));
}

void NetworkHandler::SetCookie(const std::string& name,
                               const std::string& value,
                               Maybe<std::string> url,
                               Maybe<std::string> domain,
                               Maybe<std::string> path,
                               Maybe<bool> secure,
                               Maybe<bool> http_only,
                               Maybe<std::string> same_site,
                               Maybe<double> expires,
                               std::unique_ptr<SetCookieCallback> callback) {
  if (!storage_partition_) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  if (!url.isJust() && !domain.isJust()) {
    callback->sendFailure(Response::InvalidParams(
        "At least one of the url and domain needs to be specified"));
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          &SetCookieOnIO,
          base::Unretained(storage_partition_->GetURLRequestContext()), name,
          value, url.fromMaybe(""), domain.fromMaybe(""), path.fromMaybe(""),
          secure.fromMaybe(false), http_only.fromMaybe(false),
          same_site.fromMaybe(""), expires.fromMaybe(-1),
          base::BindOnce(&CookieSetOnIO, std::move(callback))));
}

void NetworkHandler::SetCookies(
    std::unique_ptr<protocol::Array<Network::CookieParam>> cookies,
    std::unique_ptr<SetCookiesCallback> callback) {
  if (!storage_partition_) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          &SetCookiesOnIO,
          base::Unretained(storage_partition_->GetURLRequestContext()),
          std::move(cookies),
          base::BindOnce(&CookiesSetOnIO, std::move(callback))));
}

void NetworkHandler::DeleteCookies(
    const std::string& name,
    Maybe<std::string> url,
    Maybe<std::string> domain,
    Maybe<std::string> path,
    std::unique_ptr<DeleteCookiesCallback> callback) {
  if (!storage_partition_) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  if (!url.isJust() && !domain.isJust()) {
    callback->sendFailure(Response::InvalidParams(
        "At least one of the url and domain needs to be specified"));
  }
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          &DeleteCookiesOnIO,
          base::Unretained(storage_partition_->GetURLRequestContext()), name,
          url.fromMaybe(""), domain.fromMaybe(""), path.fromMaybe(""),
          base::BindOnce(&DeleteCookiesCallback::sendSuccess,
                         std::move(callback))));
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

Response NetworkHandler::SetExtraHTTPHeaders(
    std::unique_ptr<protocol::Network::Headers> headers) {
  std::vector<std::pair<std::string, std::string>> new_headers;
  std::unique_ptr<protocol::DictionaryValue> object = headers->toValue();
  for (size_t i = 0; i < object->size(); ++i) {
    auto entry = object->at(i);
    std::string value;
    if (!entry.second->asString(&value))
      return Response::InvalidParams("Invalid header value, string expected");
    if (!net::HttpUtil::IsValidHeaderName(entry.first))
      return Response::InvalidParams("Invalid header name");
    if (!net::HttpUtil::IsValidHeaderValue(value))
      return Response::InvalidParams("Invalid header value");
    new_headers.emplace_back(entry.first, value);
  }
  extra_headers_.swap(new_headers);
  return Response::FallThrough();
}

Response NetworkHandler::CanEmulateNetworkConditions(bool* result) {
  *result = true;
  return Response::OK();
}

Response NetworkHandler::EmulateNetworkConditions(
    bool offline,
    double latency,
    double download_throughput,
    double upload_throughput,
    Maybe<protocol::Network::ConnectionType>) {
  network::mojom::NetworkConditionsPtr network_conditions;
  bool throttling_enabled = offline || latency > 0 || download_throughput > 0 ||
                            upload_throughput > 0;
  if (throttling_enabled) {
    network_conditions = network::mojom::NetworkConditions::New();
    network_conditions->offline = offline;
    network_conditions->latency = base::TimeDelta::FromMilliseconds(latency);
    network_conditions->download_throughput = download_throughput;
    network_conditions->upload_throughput = upload_throughput;
  }
  SetNetworkConditions(std::move(network_conditions));
  return Response::FallThrough();
}

Response NetworkHandler::SetBypassServiceWorker(bool bypass) {
  bypass_service_worker_ = bypass;
  return Response::FallThrough();
}

namespace {

std::unique_ptr<protocol::Network::SecurityDetails> BuildSecurityDetails(
    const network::ResourceResponseInfo& info) {
  if (info.certificate.empty())
    return nullptr;
  scoped_refptr<net::X509Certificate> cert(
      net::X509Certificate::CreateFromBytes(info.certificate[0].data(),
                                            info.certificate[0].size()));
  if (!cert)
    return nullptr;

  std::unique_ptr<
      protocol::Array<protocol::Network::SignedCertificateTimestamp>>
      signed_certificate_timestamp_list =
          protocol::Array<Network::SignedCertificateTimestamp>::create();
  for (auto const& sct : info.signed_certificate_timestamps) {
    std::unique_ptr<protocol::Network::SignedCertificateTimestamp>
        signed_certificate_timestamp =
            Network::SignedCertificateTimestamp::Create()
                .SetStatus(net::ct::StatusToString(sct.status))
                .SetOrigin(net::ct::OriginToString(sct.sct->origin))
                .SetLogDescription(sct.sct->log_description)
                .SetLogId(base::HexEncode(sct.sct->log_id.c_str(),
                                          sct.sct->log_id.length()))
                .SetTimestamp(
                    (sct.sct->timestamp - base::Time()).InMillisecondsF())
                .SetHashAlgorithm(net::ct::HashAlgorithmToString(
                    sct.sct->signature.hash_algorithm))
                .SetSignatureAlgorithm(net::ct::SignatureAlgorithmToString(
                    sct.sct->signature.signature_algorithm))
                .SetSignatureData(
                    base::HexEncode(sct.sct->signature.signature_data.c_str(),
                                    sct.sct->signature.signature_data.length()))
                .Build();
    signed_certificate_timestamp_list->addItem(
        std::move(signed_certificate_timestamp));
  }
  std::vector<std::string> san_dns;
  std::vector<std::string> san_ip;
  cert->GetSubjectAltName(&san_dns, &san_ip);
  std::unique_ptr<Array<String>> san_list = Array<String>::create();
  for (const std::string& san : san_dns)
    san_list->addItem(san);
  for (const std::string& san : san_ip) {
    san_list->addItem(
        net::IPAddress(reinterpret_cast<const uint8_t*>(san.data()), san.size())
            .ToString());
  }

  int ssl_version =
      net::SSLConnectionStatusToVersion(info.ssl_connection_status);
  const char* protocol;
  net::SSLVersionToString(&protocol, ssl_version);

  const char* key_exchange;
  const char* cipher;
  const char* mac;
  bool is_aead;
  bool is_tls13;
  uint16_t cipher_suite =
      net::SSLConnectionStatusToCipherSuite(info.ssl_connection_status);
  net::SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, &is_aead,
                               &is_tls13, cipher_suite);
  if (key_exchange == nullptr) {
    DCHECK(is_tls13);
    key_exchange = "";
  }

  std::unique_ptr<protocol::Network::SecurityDetails> security_details =
      protocol::Network::SecurityDetails::Create()
          .SetProtocol(protocol)
          .SetKeyExchange(key_exchange)
          .SetCipher(cipher)
          .SetSubjectName(cert->subject().common_name)
          .SetSanList(std::move(san_list))
          .SetIssuer(cert->issuer().common_name)
          .SetValidFrom(cert->valid_start().ToDoubleT())
          .SetValidTo(cert->valid_expiry().ToDoubleT())
          .SetCertificateId(0)  // Keep this in protocol for compatability.
          .SetSignedCertificateTimestampList(
              std::move(signed_certificate_timestamp_list))
          .Build();

  if (info.ssl_key_exchange_group != 0) {
    const char* key_exchange_group =
        SSL_get_curve_name(info.ssl_key_exchange_group);
    if (key_exchange_group)
      security_details->SetKeyExchangeGroup(key_exchange_group);
  }
  if (mac)
    security_details->SetMac(mac);

  return security_details;
}

std::unique_ptr<Network::Response> BuildResponse(
    const GURL& url,
    const network::ResourceResponseInfo& info) {
  std::unique_ptr<DictionaryValue> headers_dict(DictionaryValue::create());
  int status = 0;
  std::string status_text;
  if (info.headers) {
    size_t iterator = 0;
    std::string name;
    std::string value;
    while (info.headers->EnumerateHeaderLines(&iterator, &name, &value)) {
      std::string old_value;
      bool merge_with_another = headers_dict->getString(name, &old_value);
      headers_dict->setString(
          name, merge_with_another ? old_value + '\n' + value : value);
    }
    status = info.headers->response_code();
    status_text = info.headers->GetStatusText();
  } else if (url.SchemeIs(url::kDataScheme)) {
    status = net::HTTP_OK;
    status_text = "OK";
  }

  auto response =
      Network::Response::Create()
          .SetUrl(StripFragment(url))
          .SetStatus(status)
          .SetStatusText(status_text)
          .SetHeaders(Object::fromValue(headers_dict.get(), nullptr))
          .SetMimeType(info.mime_type)
          .SetConnectionReused(info.load_timing.socket_reused)
          .SetConnectionId(info.load_timing.socket_log_id)
          .SetSecurityState(securityState(url, info.cert_status))
          .SetEncodedDataLength(info.encoded_data_length)
          .SetTiming(GetTiming(info.load_timing))
          .SetFromDiskCache(!info.load_timing.request_start_time.is_null() &&
                            info.response_time <
                                info.load_timing.request_start_time)
          .Build();
  response->SetFromServiceWorker(info.was_fetched_via_service_worker);
  network::HttpRawRequestResponseInfo* raw_info =
      info.raw_request_response_info.get();
  if (raw_info) {
    if (raw_info->http_status_code) {
      response->SetStatus(raw_info->http_status_code);
      response->SetStatusText(raw_info->http_status_text);
    }
    if (raw_info->request_headers.size()) {
      response->SetRequestHeaders(GetHeaders(raw_info->request_headers));
    }
    if (!raw_info->request_headers_text.empty()) {
      response->SetRequestHeadersText(raw_info->request_headers_text);
    }
    if (raw_info->response_headers.size())
      response->SetHeaders(GetHeaders(raw_info->response_headers));
    if (!raw_info->response_headers_text.empty())
      response->SetHeadersText(raw_info->response_headers_text);
  }
  response->SetProtocol(GetProtocol(url, info));
  response->SetRemoteIPAddress(info.socket_address.HostForURL());
  response->SetRemotePort(info.socket_address.port());
  response->SetSecurityDetails(BuildSecurityDetails(info));

  return response;
}
}  // namespace

void NetworkHandler::NavigationRequestWillBeSent(
    const NavigationRequest& nav_request) {
  if (!enabled_)
    return;

  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(nav_request.begin_params()->headers);
  std::unique_ptr<DictionaryValue> headers_dict(DictionaryValue::create());
  for (net::HttpRequestHeaders::Iterator it(headers); it.GetNext();)
    headers_dict->setString(it.name(), it.value());

  const CommonNavigationParams& common_params = nav_request.common_params();
  GURL referrer = common_params.referrer.url;
  // This is normally added down the stack, so we have to fake it here.
  if (!referrer.is_empty())
    headers_dict->setString(net::HttpRequestHeaders::kReferer, referrer.spec());

  std::unique_ptr<Network::Response> redirect_response;
  const RequestNavigationParams& request_params = nav_request.request_params();
  if (!request_params.redirect_response.empty()) {
    redirect_response = BuildResponse(request_params.redirects.back(),
                                      request_params.redirect_response.back());
  }
  auto request =
      Network::Request::Create()
          .SetUrl(StripFragment(common_params.url))
          .SetMethod(common_params.method)
          .SetHeaders(Object::fromValue(headers_dict.get(), nullptr))
          .SetInitialPriority(resourcePriority(net::HIGHEST))
          .SetReferrerPolicy(referrerPolicy(common_params.referrer.policy))
          .Build();

  std::string post_data;
  if (common_params.post_data &&
      GetPostData(*common_params.post_data, &post_data)) {
    request->SetPostData(post_data);
  }
  // TODO(caseq): report potentially blockable types
  request->SetMixedContentType(Security::MixedContentTypeEnum::None);

  std::unique_ptr<Network::Initiator> initiator;
  base::DictionaryValue* initiator_value =
      nav_request.begin_params()->devtools_initiator.get();
  if (initiator_value) {
    ErrorSupport ignored_errors;
    initiator = Network::Initiator::fromValue(
        toProtocolValue(initiator_value, 1000).get(), &ignored_errors);
  }
  if (!initiator) {
    initiator = Network::Initiator::Create()
                    .SetType(Network::Initiator::TypeEnum::Other)
                    .Build();
  }
  std::string id = nav_request.devtools_navigation_token().ToString();
  double current_ticks =
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
  double current_wall_time = base::Time::Now().ToDoubleT();
  std::string frame_token =
      nav_request.frame_tree_node()->devtools_frame_token().ToString();
  frontend_->RequestWillBeSent(
      id, id, StripFragment(common_params.url), std::move(request),
      current_ticks, current_wall_time, std::move(initiator),
      std::move(redirect_response),
      std::string(Page::ResourceTypeEnum::Document), std::move(frame_token));
}

void NetworkHandler::RequestSent(const std::string& request_id,
                                 const std::string& loader_id,
                                 const network::ResourceRequest& request,
                                 const char* initiator_type) {
  if (!enabled_)
    return;
  std::unique_ptr<DictionaryValue> headers_dict(DictionaryValue::create());
  for (net::HttpRequestHeaders::Iterator it(request.headers); it.GetNext();)
    headers_dict->setString(it.name(), it.value());
  frontend_->RequestWillBeSent(
      request_id, loader_id, StripFragment(request.url),
      Network::Request::Create()
          .SetUrl(StripFragment(request.url))
          .SetMethod(request.method)
          .SetHeaders(Object::fromValue(headers_dict.get(), nullptr))
          .SetInitialPriority(resourcePriority(request.priority))
          .SetReferrerPolicy(referrerPolicy(request.referrer_policy))
          .Build(),
      base::TimeTicks::Now().ToInternalValue() /
          static_cast<double>(base::Time::kMicrosecondsPerSecond),
      base::Time::Now().ToDoubleT(),
      Network::Initiator::Create().SetType(initiator_type).Build(),
      std::unique_ptr<Network::Response>(),
      std::string(Page::ResourceTypeEnum::Other));
}

void NetworkHandler::ResponseReceived(const std::string& request_id,
                                      const std::string& loader_id,
                                      const GURL& url,
                                      const char* resource_type,
                                      const network::ResourceResponseHead& head,
                                      Maybe<std::string> frame_id) {
  if (!enabled_)
    return;
  std::unique_ptr<Network::Response> response(BuildResponse(url, head));
  frontend_->ResponseReceived(
      request_id, loader_id,
      base::TimeTicks::Now().ToInternalValue() /
          static_cast<double>(base::Time::kMicrosecondsPerSecond),
      resource_type, std::move(response), std::move(frame_id));
}

void NetworkHandler::LoadingComplete(
    const std::string& request_id,
    const char* resource_type,
    const network::URLLoaderCompletionStatus& status) {
  if (!enabled_)
    return;

  if (status.error_code != net::OK) {
    frontend_->LoadingFailed(
        request_id,
        base::TimeTicks::Now().ToInternalValue() /
            static_cast<double>(base::Time::kMicrosecondsPerSecond),
        resource_type, net::ErrorToString(status.error_code),
        status.error_code == net::Error::ERR_ABORTED);
    return;
  }
  frontend_->LoadingFinished(
      request_id,
      status.completion_time.ToInternalValue() /
          static_cast<double>(base::Time::kMicrosecondsPerSecond),
      status.encoded_data_length);
}

void NetworkHandler::NavigationFailed(NavigationRequest* navigation_request) {
  if (!enabled_)
    return;

  static int next_id = 0;
  std::string request_id = base::IntToString(base::GetCurrentProcId()) + "." +
                           base::IntToString(++next_id);
  std::string error_string =
      net::ErrorToString(navigation_request->net_error());
  bool cancelled = navigation_request->net_error() == net::Error::ERR_ABORTED;

  std::unique_ptr<DictionaryValue> headers_dict(DictionaryValue::create());
  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(navigation_request->begin_params()->headers);
  for (net::HttpRequestHeaders::Iterator it(headers); it.GetNext();)
    headers_dict->setString(it.name(), it.value());
  frontend_->RequestWillBeSent(
      request_id, "" /* loader_id */,
      StripFragment(navigation_request->common_params().url),
      Network::Request::Create()
          .SetUrl(StripFragment(navigation_request->common_params().url))
          .SetMethod(navigation_request->common_params().method)
          .SetHeaders(Object::fromValue(headers_dict.get(), nullptr))
          // Note: the priority value is copied from
          // ResourceDispatcherHostImpl::BeginNavigationRequest but there isn't
          // a good way of sharing this.
          .SetInitialPriority(resourcePriority(net::HIGHEST))
          .SetReferrerPolicy(referrerPolicy(
              navigation_request->common_params().referrer.policy))
          .Build(),
      base::TimeTicks::Now().ToInternalValue() /
          static_cast<double>(base::Time::kMicrosecondsPerSecond),
      base::Time::Now().ToDoubleT(),
      Network::Initiator::Create()
          .SetType(Network::Initiator::TypeEnum::Parser)
          .Build(),
      std::unique_ptr<Network::Response>(),
      std::string(Page::ResourceTypeEnum::Document));

  frontend_->LoadingFailed(
      request_id,
      base::TimeTicks::Now().ToInternalValue() /
          static_cast<double>(base::Time::kMicrosecondsPerSecond),
      Page::ResourceTypeEnum::Document, error_string, cancelled);
}

DispatchResponse NetworkHandler::SetRequestInterception(
    std::unique_ptr<protocol::Array<protocol::Network::RequestPattern>>
        patterns) {
  WebContents* web_contents = WebContents::FromRenderFrameHost(host_);
  if (!web_contents)
    return Response::InternalError();

  DevToolsInterceptorController* interceptor =
      DevToolsInterceptorController::FromBrowserContext(
          web_contents->GetBrowserContext());
  if (!interceptor)
    return Response::Error("Interception not supported");

  if (!patterns->length()) {
    interception_handle_.reset();
    return Response::OK();
  }

  std::vector<DevToolsNetworkInterceptor::Pattern> interceptor_patterns;
  for (size_t i = 0; i < patterns->length(); ++i) {
    base::flat_set<ResourceType> resource_types;
    std::string resource_type = patterns->get(i)->GetResourceType("");
    if (!resource_type.empty()) {
      if (!AddInterceptedResourceType(resource_type, &resource_types)) {
        return Response::InvalidParams(base::StringPrintf(
            "Cannot intercept resources of type '%s'", resource_type.c_str()));
      }
    }
    interceptor_patterns.push_back(DevToolsNetworkInterceptor::Pattern(
        patterns->get(i)->GetUrlPattern("*"), std::move(resource_types),
        ToInterceptorStage(patterns->get(i)->GetInterceptionStage(
            protocol::Network::InterceptionStageEnum::Request))));
  }

  if (interception_handle_) {
    interception_handle_->UpdatePatterns(std::move(interceptor_patterns));
  } else {
    interception_handle_ = interceptor->StartInterceptingRequests(
        host_->frame_tree_node(), std::move(interceptor_patterns),
        base::Bind(&NetworkHandler::RequestIntercepted,
                   weak_factory_.GetWeakPtr()));
  }

  return Response::OK();
}

void NetworkHandler::ContinueInterceptedRequest(
    const std::string& interception_id,
    Maybe<std::string> error_reason,
    Maybe<std::string> base64_raw_response,
    Maybe<std::string> url,
    Maybe<std::string> method,
    Maybe<std::string> post_data,
    Maybe<protocol::Network::Headers> headers,
    Maybe<protocol::Network::AuthChallengeResponse> auth_challenge_response,
    std::unique_ptr<ContinueInterceptedRequestCallback> callback) {
  DevToolsInterceptorController* interceptor =
      DevToolsInterceptorController::FromBrowserContext(browser_context_);
  if (!interceptor) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  base::Optional<std::string> raw_response;
  if (base64_raw_response.isJust()) {
    std::string decoded;
    if (!base::Base64Decode(base64_raw_response.fromJust(), &decoded)) {
      callback->sendFailure(Response::InvalidParams("Invalid rawResponse."));
      return;
    }
    raw_response = decoded;
  }

  base::Optional<net::Error> error;
  bool mark_as_canceled = false;
  if (error_reason.isJust()) {
    bool ok;
    error = NetErrorFromString(error_reason.fromJust(), &ok);
    if (!ok) {
      callback->sendFailure(Response::InvalidParams("Invalid errorReason."));
      return;
    }

    mark_as_canceled = true;
  }

  interceptor->ContinueInterceptedRequest(
      interception_id,
      std::make_unique<DevToolsNetworkInterceptor::Modifications>(
          std::move(error), std::move(raw_response), std::move(url),
          std::move(method), std::move(post_data), std::move(headers),
          std::move(auth_challenge_response), mark_as_canceled),
      std::move(callback));
}

void NetworkHandler::GetResponseBodyForInterception(
    const String& interception_id,
    std::unique_ptr<GetResponseBodyForInterceptionCallback> callback) {
  DevToolsInterceptorController* interceptor =
      DevToolsInterceptorController::FromBrowserContext(browser_context_);
  if (!interceptor) {
    callback->sendFailure(Response::InternalError());
    return;
  }
  interceptor->GetResponseBody(interception_id, std::move(callback));
}

// static
GURL NetworkHandler::ClearUrlRef(const GURL& url) {
  if (!url.has_ref())
    return url;
  GURL::Replacements replacements;
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

// static
std::unique_ptr<Network::Request> NetworkHandler::CreateRequestFromURLRequest(
    const net::URLRequest* request) {
  std::unique_ptr<DictionaryValue> headers_dict(DictionaryValue::create());
  for (net::HttpRequestHeaders::Iterator it(request->extra_request_headers());
       it.GetNext();) {
    headers_dict->setString(it.name(), it.value());
  }
  if (!request->referrer().empty()) {
    headers_dict->setString(net::HttpRequestHeaders::kReferer,
                            request->referrer());
  }
  std::unique_ptr<protocol::Network::Request> request_object =
      Network::Request::Create()
          .SetUrl(ClearUrlRef(request->url()).spec())
          .SetMethod(request->method())
          .SetHeaders(Object::fromValue(headers_dict.get(), nullptr))
          .SetInitialPriority(resourcePriority(request->priority()))
          .SetReferrerPolicy(referrerPolicy(request->referrer_policy()))
          .Build();
  std::string post_data;
  if (GetPostData(request, &post_data))
    request_object->SetPostData(std::move(post_data));
  return request_object;
}

std::unique_ptr<NavigationThrottle> NetworkHandler::CreateThrottleForNavigation(
    NavigationHandle* navigation_handle) {
  if (!interception_handle_)
    return nullptr;
  std::unique_ptr<NavigationThrottle> throttle(new NetworkNavigationThrottle(
      weak_factory_.GetWeakPtr(), navigation_handle));
  return throttle;
}

bool NetworkHandler::ShouldCancelNavigation(
    const GlobalRequestID& global_request_id) {
  WebContents* web_contents = WebContents::FromRenderFrameHost(host_);
  if (!web_contents)
    return false;
  DevToolsInterceptorController* interceptor =
      DevToolsInterceptorController::FromBrowserContext(
          web_contents->GetBrowserContext());
  return interceptor && interceptor->ShouldCancelNavigation(global_request_id);
}

void NetworkHandler::ApplyOverrides(net::HttpRequestHeaders* headers,
                                    bool* skip_service_worker,
                                    bool* disable_cache) {
  headers->SetHeader(kDevToolsEmulateNetworkConditionsClientId, host_id_);
  if (!user_agent_.empty())
    headers->SetHeader(net::HttpRequestHeaders::kUserAgent, user_agent_);
  for (auto& entry : extra_headers_)
    headers->SetHeader(entry.first, entry.second);
  *skip_service_worker |= bypass_service_worker_;
  *disable_cache |= cache_disabled_;
}

namespace {

const char* ResourceTypeToString(ResourceType resource_type) {
  switch (resource_type) {
    case RESOURCE_TYPE_MAIN_FRAME:
      return protocol::Page::ResourceTypeEnum::Document;
    case RESOURCE_TYPE_SUB_FRAME:
      return protocol::Page::ResourceTypeEnum::Document;
    case RESOURCE_TYPE_STYLESHEET:
      return protocol::Page::ResourceTypeEnum::Stylesheet;
    case RESOURCE_TYPE_SCRIPT:
      return protocol::Page::ResourceTypeEnum::Script;
    case RESOURCE_TYPE_IMAGE:
      return protocol::Page::ResourceTypeEnum::Image;
    case RESOURCE_TYPE_FONT_RESOURCE:
      return protocol::Page::ResourceTypeEnum::Font;
    case RESOURCE_TYPE_SUB_RESOURCE:
      return protocol::Page::ResourceTypeEnum::Other;
    case RESOURCE_TYPE_OBJECT:
      return protocol::Page::ResourceTypeEnum::Other;
    case RESOURCE_TYPE_MEDIA:
      return protocol::Page::ResourceTypeEnum::Media;
    case RESOURCE_TYPE_WORKER:
      return protocol::Page::ResourceTypeEnum::Other;
    case RESOURCE_TYPE_SHARED_WORKER:
      return protocol::Page::ResourceTypeEnum::Other;
    case RESOURCE_TYPE_PREFETCH:
      return protocol::Page::ResourceTypeEnum::Fetch;
    case RESOURCE_TYPE_FAVICON:
      return protocol::Page::ResourceTypeEnum::Other;
    case RESOURCE_TYPE_XHR:
      return protocol::Page::ResourceTypeEnum::XHR;
    case RESOURCE_TYPE_PING:
      return protocol::Page::ResourceTypeEnum::Other;
    case RESOURCE_TYPE_SERVICE_WORKER:
      return protocol::Page::ResourceTypeEnum::Other;
    case RESOURCE_TYPE_CSP_REPORT:
      return protocol::Page::ResourceTypeEnum::Other;
    case RESOURCE_TYPE_PLUGIN_RESOURCE:
      return protocol::Page::ResourceTypeEnum::Other;
    default:
      return protocol::Page::ResourceTypeEnum::Other;
  }
}

}  // namespace

void NetworkHandler::RequestIntercepted(
    std::unique_ptr<InterceptedRequestInfo> info) {
  protocol::Maybe<protocol::Network::ErrorReason> error_reason;
  if (info->response_error_code < 0)
    error_reason = NetErrorToString(info->response_error_code);
  frontend_->RequestIntercepted(
      info->interception_id, std::move(info->network_request),
      info->frame_id.ToString(), ResourceTypeToString(info->resource_type),
      info->is_navigation, std::move(info->redirect_url),
      std::move(info->auth_challenge), std::move(error_reason),
      std::move(info->http_response_status_code),
      std::move(info->response_headers));
}

void NetworkHandler::SetNetworkConditions(
    network::mojom::NetworkConditionsPtr conditions) {
  if (!storage_partition_)
    return;
  network::mojom::NetworkContext* context =
      storage_partition_->GetNetworkContext();
  context->SetNetworkConditions(host_id_, std::move(conditions));
}

}  // namespace protocol
}  // namespace content
