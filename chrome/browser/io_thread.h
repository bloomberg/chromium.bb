// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IO_THREAD_H_
#define CHROME_BROWSER_IO_THREAD_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/net/ssl_config_service_manager.h"
#include "chrome/browser/prefs/pref_member.h"
#include "content/browser/browser_process_sub_thread.h"
#include "net/base/network_change_notifier.h"

class ChromeNetLog;
class ExtensionEventRouterForwarder;
class MediaInternals;
class PrefProxyConfigTrackerImpl;
class PrefService;
class SystemURLRequestContextGetter;

namespace net {
class CertVerifier;
class CookieStore;
class DnsRRResolver;
class FtpTransactionFactory;
class HostResolver;
class HttpAuthHandlerFactory;
class HttpServerProperties;
class HttpTransactionFactory;
class NetworkDelegate;
class OriginBoundCertService;
class ProxyConfigService;
class ProxyService;
class SdchManager;
class SSLConfigService;
class URLRequestContext;
class URLRequestContextGetter;
class URLSecurityManager;
}  // namespace net

class IOThread : public content::BrowserProcessSubThread {
 public:
  struct Globals {
    Globals();
    ~Globals();

    struct MediaGlobals {
      MediaGlobals();
      ~MediaGlobals();
      // MediaInternals singleton used to aggregate media information.
      scoped_ptr<MediaInternals> media_internals;
    } media;

    // The "system" NetworkDelegate, used for Profile-agnostic network events.
    scoped_ptr<net::NetworkDelegate> system_network_delegate;
    scoped_ptr<net::HostResolver> host_resolver;
    scoped_ptr<net::CertVerifier> cert_verifier;
    scoped_ptr<net::DnsRRResolver> dnsrr_resolver;
    scoped_refptr<net::SSLConfigService> ssl_config_service;
    scoped_ptr<net::HttpAuthHandlerFactory> http_auth_handler_factory;
    scoped_ptr<net::HttpServerProperties> http_server_properties;
    scoped_ptr<net::ProxyService> proxy_script_fetcher_proxy_service;
    scoped_ptr<net::HttpTransactionFactory>
        proxy_script_fetcher_http_transaction_factory;
    scoped_ptr<net::FtpTransactionFactory>
        proxy_script_fetcher_ftp_transaction_factory;
    scoped_ptr<net::URLSecurityManager> url_security_manager;
    // We use a separate URLRequestContext for PAC fetches, in order to break
    // the reference cycle:
    // URLRequestContext=>PAC fetch=>URLRequest=>URLRequestContext.
    // The first URLRequestContext is |system_url_request_context|. We introduce
    // |proxy_script_fetcher_context| for the second context. It has a direct
    // ProxyService, since we always directly connect to fetch the PAC script.
    scoped_refptr<net::URLRequestContext> proxy_script_fetcher_context;
    scoped_ptr<net::ProxyService> system_proxy_service;
    scoped_ptr<net::HttpTransactionFactory> system_http_transaction_factory;
    scoped_ptr<net::FtpTransactionFactory> system_ftp_transaction_factory;
    scoped_refptr<net::URLRequestContext> system_request_context;
    // |cookie_store| and |origin_bound_cert_service| are shared between
    // |proxy_script_fetcher_context| and |system_request_context|.
    scoped_refptr<net::CookieStore> system_cookie_store;
    scoped_ptr<net::OriginBoundCertService> system_origin_bound_cert_service;
    scoped_refptr<ExtensionEventRouterForwarder>
        extension_event_router_forwarder;
  };

  // |net_log| must either outlive the IOThread or be NULL.
  IOThread(PrefService* local_state,
           ChromeNetLog* net_log,
           ExtensionEventRouterForwarder* extension_event_router_forwarder);

  virtual ~IOThread();

  // Can only be called on the IO thread.
  Globals* globals();

  ChromeNetLog* net_log();

  // Returns a getter for the URLRequestContext.  Only called on the UI thread.
  net::URLRequestContextGetter* system_url_request_context_getter();

  // Clears the host cache.  Intended to be used to prevent exposing recently
  // visited sites on about:net-internals/#dns and about:dns pages.  Must be
  // called on the IO thread.
  void ClearHostCache();

 protected:
  virtual void Init();
  virtual void CleanUp();

 private:
  // Provide SystemURLRequestContextGetter with access to
  // InitSystemRequestContext().
  friend class SystemURLRequestContextGetter;

  static void RegisterPrefs(PrefService* local_state);

  net::HttpAuthHandlerFactory* CreateDefaultAuthHandlerFactory(
      net::HostResolver* resolver);

  void InitSystemRequestContext();

  // Lazy initialization of system request context for
  // SystemURLRequestContextGetter. To be called on IO thread.
  void InitSystemRequestContextOnIOThread();

  // Returns an SSLConfigService instance.
  net::SSLConfigService* GetSSLConfigService();

  // The NetLog is owned by the browser process, to allow logging from other
  // threads during shutdown, but is used most frequently on the IOThread.
  ChromeNetLog* net_log_;

  // The ExtensionEventRouterForwarder allows for sending events to extensions
  // from the IOThread.
  ExtensionEventRouterForwarder* extension_event_router_forwarder_;

  // These member variables are basically global, but their lifetimes are tied
  // to the IOThread.  IOThread owns them all, despite not using scoped_ptr.
  // This is because the destructor of IOThread runs on the wrong thread.  All
  // member variables should be deleted in CleanUp().

  // These member variables are initialized in Init() and do not change for the
  // lifetime of the IO thread.

  Globals* globals_;

  // Observer that logs network changes to the ChromeNetLog.
  scoped_ptr<net::NetworkChangeNotifier::IPAddressObserver>
      network_change_observer_;

  BooleanPrefMember system_enable_referrers_;

  // Store HTTP Auth-related policies in this thread.
  std::string auth_schemes_;
  bool negotiate_disable_cname_lookup_;
  bool negotiate_enable_port_;
  std::string auth_server_whitelist_;
  std::string auth_delegate_whitelist_;
  std::string gssapi_library_name_;

  // This is an instance of the default SSLConfigServiceManager for the current
  // platform and it gets SSL preferences from local_state object.
  scoped_ptr<SSLConfigServiceManager> ssl_config_service_manager_;

  // These member variables are initialized by a task posted to the IO thread,
  // which gets posted by calling certain member functions of IOThread.
  scoped_ptr<net::ProxyConfigService> system_proxy_config_service_;

  scoped_ptr<PrefProxyConfigTrackerImpl> pref_proxy_config_tracker_;

  scoped_refptr<net::URLRequestContextGetter>
      system_url_request_context_getter_;

  net::SdchManager* sdch_manager_;

  ScopedRunnableMethodFactory<IOThread> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(IOThread);
};

#endif  // CHROME_BROWSER_IO_THREAD_H_
