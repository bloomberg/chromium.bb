// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NET_AW_URL_REQUEST_CONTEXT_GETTER_H_
#define ANDROID_WEBVIEW_BROWSER_NET_AW_URL_REQUEST_CONTEXT_GETTER_H_

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/prefs/pref_member.h"
#include "content/public/browser/content_browser_client.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory.h"

class PrefService;

namespace net {
class CookieStore;
class HostResolver;
class HttpAuthHandlerFactory;
class HttpAuthPreferences;
class HttpUserAgentSettings;
class NetLog;
class ProxyConfigService;
class URLRequestContext;
class URLRequestJobFactory;
}

namespace android_webview {

class AwNetworkDelegate;

class AwURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  AwURLRequestContextGetter(
      const base::FilePath& cache_path,
      net::CookieStore* cookie_store,
      scoped_ptr<net::ProxyConfigService> config_service,
      PrefService* pref_service);

  // net::URLRequestContextGetter implementation.
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

  // NetLog is thread-safe, so clients can call this method from arbitrary
  // threads (UI and IO).
  net::NetLog* GetNetLog();

  // This should be called before the network stack is ever used. It can be
  // called again afterwards if the key updates.
  void SetKeyOnIO(const std::string& key);

 private:
  friend class AwBrowserContext;
  ~AwURLRequestContextGetter() override;

  // Prior to GetURLRequestContext() being called, this is called to hand over
  // the objects that GetURLRequestContext() will later install into
  // |job_factory_|.  This ordering is enforced by having
  // AwBrowserContext::CreateRequestContext() call this method.
  // This method is necessary because the passed in objects are created
  // on the UI thread while |job_factory_| must be created on the IO thread.
  void SetHandlersAndInterceptors(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors);

  void InitializeURLRequestContext();

  // This is called to create a HttpAuthHandlerFactory that will handle
  // auth challenges for the new URLRequestContext
  scoped_ptr<net::HttpAuthHandlerFactory> CreateAuthHandlerFactory(
      net::HostResolver* resolver);

  // Update methods for the auth related preferences
  void UpdateServerWhitelist();
  void UpdateAndroidAuthNegotiateAccountType();

  const base::FilePath cache_path_;

  scoped_ptr<net::NetLog> net_log_;
  scoped_ptr<net::ProxyConfigService> proxy_config_service_;
  scoped_refptr<net::CookieStore> cookie_store_;
  scoped_ptr<net::URLRequestJobFactory> job_factory_;
  scoped_ptr<net::HttpUserAgentSettings> http_user_agent_settings_;
  // http_auth_preferences_ holds the preferences for the negotiate
  // authenticator.
  scoped_ptr<net::HttpAuthPreferences> http_auth_preferences_;
  scoped_ptr<net::URLRequestContext> url_request_context_;

  // Store HTTP Auth-related policies in this thread.
  StringPrefMember auth_android_negotiate_account_type_;
  StringPrefMember auth_server_whitelist_;

  // ProtocolHandlers and interceptors are stored here between
  // SetHandlersAndInterceptors() and the first GetURLRequestContext() call.
  content::ProtocolHandlerMap protocol_handlers_;
  content::URLRequestInterceptorScopedVector request_interceptors_;

  DISALLOW_COPY_AND_ASSIGN(AwURLRequestContextGetter);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NET_AW_URL_REQUEST_CONTEXT_GETTER_H_
