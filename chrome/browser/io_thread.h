// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IO_THREAD_H_
#define CHROME_BROWSER_IO_THREAD_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/net/ssl_config_service_manager.h"
#include "chrome/browser/prefs/pref_member.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_thread_delegate.h"
#include "net/base/network_change_notifier.h"

class ChromeNetLog;
class ExtensionEventRouterForwarder;
class PrefProxyConfigTrackerImpl;
class PrefService;
class SystemURLRequestContextGetter;

namespace chrome_browser_net {
class HttpPipeliningCompatibilityClient;
}

namespace net {
class CertVerifier;
class CookieStore;
class FtpTransactionFactory;
class HostResolver;
class HttpAuthHandlerFactory;
class HttpServerProperties;
class HttpTransactionFactory;
class NetworkDelegate;
class ServerBoundCertService;
class ProxyConfigService;
class ProxyService;
class SdchManager;
class SSLConfigService;
class TransportSecurityState;
class URLRequestContext;
class URLRequestContextGetter;
class URLRequestThrottlerManager;
class URLSecurityManager;
}  // namespace net

// Contains state associated with, initialized and cleaned up on, and
// primarily used on, the IO thread.
//
// If you are looking to interact with the IO thread (e.g. post tasks
// to it or check if it is the current thread), see
// content::BrowserThread.
class IOThread : public content::BrowserThreadDelegate {
 public:
  struct Globals {
    class SystemRequestContextLeakChecker {
     public:
      explicit SystemRequestContextLeakChecker(Globals* globals);
      ~SystemRequestContextLeakChecker();

     private:
      Globals* const globals_;
    };

    Globals();
    ~Globals();

    // The "system" NetworkDelegate, used for Profile-agnostic network events.
    scoped_ptr<net::NetworkDelegate> system_network_delegate;
    scoped_ptr<net::HostResolver> host_resolver;
    scoped_ptr<net::CertVerifier> cert_verifier;
    // This TransportSecurityState doesn't load or save any state. It's only
    // used to enforce pinning for system requests and will only use built-in
    // pins.
    scoped_ptr<net::TransportSecurityState> transport_security_state;
    scoped_refptr<net::SSLConfigService> ssl_config_service;
    scoped_ptr<net::HttpAuthHandlerFactory> http_auth_handler_factory;
    scoped_ptr<net::HttpServerProperties> http_server_properties;
    scoped_ptr<net::ProxyService> proxy_script_fetcher_proxy_service;
    scoped_ptr<net::HttpTransactionFactory>
        proxy_script_fetcher_http_transaction_factory;
    scoped_ptr<net::FtpTransactionFactory>
        proxy_script_fetcher_ftp_transaction_factory;
    scoped_ptr<net::URLRequestThrottlerManager> throttler_manager;
    scoped_ptr<net::URLSecurityManager> url_security_manager;
    // TODO(willchan): Remove proxy script fetcher context since it's not
    // necessary now that I got rid of refcounting URLRequestContexts.
    //
    // The first URLRequestContext is |system_url_request_context|. We introduce
    // |proxy_script_fetcher_context| for the second context. It has a direct
    // ProxyService, since we always directly connect to fetch the PAC script.
    scoped_ptr<net::URLRequestContext> proxy_script_fetcher_context;
    scoped_ptr<net::ProxyService> system_proxy_service;
    scoped_ptr<net::HttpTransactionFactory> system_http_transaction_factory;
    scoped_ptr<net::FtpTransactionFactory> system_ftp_transaction_factory;
    scoped_ptr<net::URLRequestContext> system_request_context;
    SystemRequestContextLeakChecker system_request_context_leak_checker;
    // |system_cookie_store| and |system_server_bound_cert_service| are shared
    // between |proxy_script_fetcher_context| and |system_request_context|.
    scoped_refptr<net::CookieStore> system_cookie_store;
    scoped_ptr<net::ServerBoundCertService> system_server_bound_cert_service;
    scoped_refptr<ExtensionEventRouterForwarder>
        extension_event_router_forwarder;
    scoped_ptr<chrome_browser_net::HttpPipeliningCompatibilityClient>
        http_pipelining_compatibility_client;
  };

  // |net_log| must either outlive the IOThread or be NULL.
  IOThread(PrefService* local_state,
           ChromeNetLog* net_log,
           ExtensionEventRouterForwarder* extension_event_router_forwarder);

  virtual ~IOThread();

  // Can only be called on the IO thread.
  Globals* globals();

  ChromeNetLog* net_log();

  // Handles changing to On The Record mode, discarding confidential data.
  void ChangedToOnTheRecord();

  // Returns a getter for the URLRequestContext.  Only called on the UI thread.
  net::URLRequestContextGetter* system_url_request_context_getter();

  // Clears the host cache.  Intended to be used to prevent exposing recently
  // visited sites on about:net-internals/#dns and about:dns pages.  Must be
  // called on the IO thread.
  void ClearHostCache();

 private:
  // BrowserThreadDelegate implementation, runs on the IO thread.
  // This handles initialization and destruction of state that must
  // live on the IO thread.
  virtual void Init() OVERRIDE;
  virtual void CleanUp() OVERRIDE;

  // Provide SystemURLRequestContextGetter with access to
  // InitSystemRequestContext().
  friend class SystemURLRequestContextGetter;

  // Global state must be initialized on the IO thread, then this
  // method must be invoked on the UI thread.
  void InitSystemRequestContext();

  // Lazy initialization of system request context for
  // SystemURLRequestContextGetter. To be called on IO thread only
  // after global state has been initialized on the IO thread, and
  // SystemRequestContext state has been initialized on the UI thread.
  void InitSystemRequestContextOnIOThread();

  static void RegisterPrefs(PrefService* local_state);

  net::HttpAuthHandlerFactory* CreateDefaultAuthHandlerFactory(
      net::HostResolver* resolver);

  // Returns an SSLConfigService instance.
  net::SSLConfigService* GetSSLConfigService();

  void ChangedToOnTheRecordOnIOThread();

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

  base::WeakPtrFactory<IOThread> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IOThread);
};

#endif  // CHROME_BROWSER_IO_THREAD_H_
