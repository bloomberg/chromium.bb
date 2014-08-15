// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_NETWORK_DELEGATE_H_
#define CHROME_BROWSER_NET_CHROME_NETWORK_DELEGATE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_metrics.h"
#include "net/base/network_delegate.h"
#include "net/proxy/proxy_retry_info.h"

class ChromeExtensionsNetworkDelegate;
class ClientHints;
class CookieSettings;
class PrefService;

template<class T> class PrefMember;

typedef PrefMember<bool> BooleanPrefMember;

namespace base {
class Value;
}

namespace chrome_browser_net {
class ConnectInterceptor;
class Predictor;
}

namespace data_reduction_proxy {
class DataReductionProxyAuthRequestHandler;
class DataReductionProxyParams;
class DataReductionProxyUsageStats;
}

namespace domain_reliability {
class DomainReliabilityMonitor;
}  // namespace domain_reliability

namespace extensions {
class EventRouterForwarder;
class InfoMap;
}

namespace net {
class ProxyConfig;
class ProxyInfo;
class ProxyServer;
class ProxyService;
class URLRequest;
}

namespace policy {
class URLBlacklistManager;
}

namespace prerender {
class PrerenderTracker;
}

// ChromeNetworkDelegate is the central point from within the chrome code to
// add hooks into the network stack.
class ChromeNetworkDelegate : public net::NetworkDelegate {
 public:
  // Provides an opportunity to interpose on proxy resolution. Called before
  // ProxyService.ResolveProxy() returns. |proxy_info| contains information
  // about the proxy being used, and may be modified by this callback.
  typedef base::Callback<void(
      const GURL& url,
      int load_flags,
      const net::ProxyConfig& data_reduction_proxy_config,
      const net::ProxyRetryInfoMap& proxy_retry_info_map,
      const data_reduction_proxy::DataReductionProxyParams* params,
      net::ProxyInfo* result)> OnResolveProxyHandler;

  // Provides an additional proxy configuration that can be consulted after
  // proxy resolution.
  typedef base::Callback<const net::ProxyConfig&()> ProxyConfigGetter;

  // |enable_referrers| (and all of the other optional PrefMembers) should be
  // initialized on the UI thread (see below) beforehand. This object's owner is
  // responsible for cleaning them up at shutdown.
  ChromeNetworkDelegate(extensions::EventRouterForwarder* event_router,
                        BooleanPrefMember* enable_referrers);
  virtual ~ChromeNetworkDelegate();

  // Pass through to ChromeExtensionsNetworkDelegate::set_extension_info_map().
  void set_extension_info_map(extensions::InfoMap* extension_info_map);

#if defined(ENABLE_CONFIGURATION_POLICY)
  void set_url_blacklist_manager(
      const policy::URLBlacklistManager* url_blacklist_manager) {
    url_blacklist_manager_ = url_blacklist_manager;
  }
#endif

  // If |profile| is NULL or not set, events will be broadcast to all profiles,
  // otherwise they will only be sent to the specified profile.
  // Also pass through to ChromeExtensionsNetworkDelegate::set_profile().
  void set_profile(void* profile);

  // |profile_path| is used to locate the "Downloads" folder on Chrome OS. If it
  // is set, the location of the Downloads folder for the profile is added to
  // the whitelist for accesses via file: scheme.
  void set_profile_path(const base::FilePath& profile_path) {
    profile_path_ = profile_path;
  }

  // If |cookie_settings| is NULL or not set, all cookies are enabled,
  // otherwise the settings are enforced on all observed network requests.
  // Not inlined because we assign a scoped_refptr, which requires us to include
  // the header file. Here we just forward-declare it.
  void set_cookie_settings(CookieSettings* cookie_settings);

  // Causes requested URLs to be fed to |predictor| via ConnectInterceptor.
  void set_predictor(chrome_browser_net::Predictor* predictor);

  void set_enable_do_not_track(BooleanPrefMember* enable_do_not_track) {
    enable_do_not_track_ = enable_do_not_track;
  }

  void set_force_google_safe_search(
      BooleanPrefMember* force_google_safe_search) {
    force_google_safe_search_ = force_google_safe_search;
  }

  void set_data_reduction_proxy_enabled_pref(
      BooleanPrefMember* data_reduction_proxy_enabled) {
    data_reduction_proxy_enabled_ = data_reduction_proxy_enabled;
  }

  void set_domain_reliability_monitor(
      domain_reliability::DomainReliabilityMonitor* monitor) {
    domain_reliability_monitor_ = monitor;
  }

  void set_prerender_tracker(prerender::PrerenderTracker* prerender_tracker) {
    prerender_tracker_ = prerender_tracker;
  }

  // |data_reduction_proxy_params_| must outlive this ChromeNetworkDelegate.
  void set_data_reduction_proxy_params(
      data_reduction_proxy::DataReductionProxyParams* params) {
    data_reduction_proxy_params_ = params;
  }

  // |data_reduction_proxy_usage_stats_| must outlive this
  // ChromeNetworkDelegate.
  void set_data_reduction_proxy_usage_stats(
      data_reduction_proxy::DataReductionProxyUsageStats* usage_stats) {
    data_reduction_proxy_usage_stats_ = usage_stats;
  }

  // |data_reduction_proxy_auth_request_handler_| must outlive this
  // ChromeNetworkDelegate.
  void set_data_reduction_proxy_auth_request_handler(
      data_reduction_proxy::DataReductionProxyAuthRequestHandler* handler) {
    data_reduction_proxy_auth_request_handler_ = handler;
  }

  void set_on_resolve_proxy_handler(OnResolveProxyHandler handler) {
    on_resolve_proxy_handler_ = handler;
  }

  void set_proxy_config_getter(const ProxyConfigGetter& getter) {
    proxy_config_getter_ = getter;
  }

  // Adds the Client Hints header to HTTP requests.
  void SetEnableClientHints();

  // Causes |OnCanThrottleRequest| to always return false, for all
  // instances of this object.
  static void NeverThrottleRequests();

  // Binds the pref members to |pref_service| and moves them to the IO thread.
  // |enable_referrers| cannot be NULL, the others can.
  // This method should be called on the UI thread.
  static void InitializePrefsOnUIThread(
      BooleanPrefMember* enable_referrers,
      BooleanPrefMember* enable_do_not_track,
      BooleanPrefMember* force_google_safe_search,
      PrefService* pref_service);

  // When called, all file:// URLs will now be accessible.  If this is not
  // called, then some platforms restrict access to file:// paths.
  static void AllowAccessToAllFiles();

  // Creates a Value summary of the persistent state of the network session.
  // The caller is responsible for deleting the returned value.
  // Must be called on the UI thread.
  static base::Value* HistoricNetworkStatsInfoToValue();

  // Creates a Value summary of the state of the network session. The caller is
  // responsible for deleting the returned value.
  base::Value* SessionNetworkStatsInfoToValue() const;

 private:
  friend class ChromeNetworkDelegateTest;

  // NetworkDelegate implementation.
  virtual int OnBeforeURLRequest(net::URLRequest* request,
                                 const net::CompletionCallback& callback,
                                 GURL* new_url) OVERRIDE;
  virtual void OnResolveProxy(
      const GURL& url,
      int load_flags,
      const net::ProxyService& proxy_service,
      net::ProxyInfo* result) OVERRIDE;
  virtual void OnProxyFallback(const net::ProxyServer& bad_proxy,
                               int net_error) OVERRIDE;
  virtual int OnBeforeSendHeaders(net::URLRequest* request,
                                  const net::CompletionCallback& callback,
                                  net::HttpRequestHeaders* headers) OVERRIDE;
  virtual void OnBeforeSendProxyHeaders(
      net::URLRequest* request,
      const net::ProxyInfo& proxy_info,
      net::HttpRequestHeaders* headers) OVERRIDE;
  virtual void OnSendHeaders(net::URLRequest* request,
                             const net::HttpRequestHeaders& headers) OVERRIDE;
  virtual int OnHeadersReceived(
      net::URLRequest* request,
      const net::CompletionCallback& callback,
      const net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
      GURL* allowed_unsafe_redirect_url) OVERRIDE;
  virtual void OnBeforeRedirect(net::URLRequest* request,
                                const GURL& new_location) OVERRIDE;
  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE;
  virtual void OnRawBytesRead(const net::URLRequest& request,
                              int bytes_read) OVERRIDE;
  virtual void OnCompleted(net::URLRequest* request, bool started) OVERRIDE;
  virtual void OnURLRequestDestroyed(net::URLRequest* request) OVERRIDE;
  virtual void OnPACScriptError(int line_number,
                                const base::string16& error) OVERRIDE;
  virtual net::NetworkDelegate::AuthRequiredResponse OnAuthRequired(
      net::URLRequest* request,
      const net::AuthChallengeInfo& auth_info,
      const AuthCallback& callback,
      net::AuthCredentials* credentials) OVERRIDE;
  virtual bool OnCanGetCookies(const net::URLRequest& request,
                               const net::CookieList& cookie_list) OVERRIDE;
  virtual bool OnCanSetCookie(const net::URLRequest& request,
                              const std::string& cookie_line,
                              net::CookieOptions* options) OVERRIDE;
  virtual bool OnCanAccessFile(const net::URLRequest& request,
                               const base::FilePath& path) const OVERRIDE;
  virtual bool OnCanThrottleRequest(
      const net::URLRequest& request) const OVERRIDE;
  virtual bool OnCanEnablePrivacyMode(
      const GURL& url,
      const GURL& first_party_for_cookies) const OVERRIDE;
  virtual int OnBeforeSocketStreamConnect(
      net::SocketStream* stream,
      const net::CompletionCallback& callback) OVERRIDE;

  void AccumulateContentLength(
      int64 received_payload_byte_count,
      int64 original_payload_byte_count,
      data_reduction_proxy::DataReductionProxyRequestType request_type);

  scoped_ptr<ChromeExtensionsNetworkDelegate> extensions_delegate_;

  void* profile_;
  base::FilePath profile_path_;
  scoped_refptr<CookieSettings> cookie_settings_;

  scoped_ptr<chrome_browser_net::ConnectInterceptor> connect_interceptor_;

  // Weak, owned by our owner.
  BooleanPrefMember* enable_referrers_;
  BooleanPrefMember* enable_do_not_track_;
  BooleanPrefMember* force_google_safe_search_;
  BooleanPrefMember* data_reduction_proxy_enabled_;

  // Weak, owned by our owner.
#if defined(ENABLE_CONFIGURATION_POLICY)
  const policy::URLBlacklistManager* url_blacklist_manager_;
#endif
  domain_reliability::DomainReliabilityMonitor* domain_reliability_monitor_;

  // When true, allow access to all file:// URLs.
  static bool g_allow_file_access_;

  // True if OnCanThrottleRequest should always return false.
  //
  // Note: This needs to be static as the instance of
  // ChromeNetworkDelegate used may change over time, and we need to
  // set this variable once at start-up time.  It is effectively
  // static anyway since it is based on a command-line flag.
  static bool g_never_throttle_requests_;

  // Total size of all content (excluding headers) that has been received
  // over the network.
  int64 received_content_length_;

  // Total original size of all content before it was transferred.
  int64 original_content_length_;

  scoped_ptr<ClientHints> client_hints_;

  bool first_request_;

  prerender::PrerenderTracker* prerender_tracker_;

  // |data_reduction_proxy_params_| must outlive this ChromeNetworkDelegate.
  data_reduction_proxy::DataReductionProxyParams* data_reduction_proxy_params_;
  // |data_reduction_proxy_usage_stats_| must outlive this
  // ChromeNetworkDelegate.
  data_reduction_proxy::DataReductionProxyUsageStats*
      data_reduction_proxy_usage_stats_;
  data_reduction_proxy::DataReductionProxyAuthRequestHandler*
  data_reduction_proxy_auth_request_handler_;

  OnResolveProxyHandler on_resolve_proxy_handler_;
  ProxyConfigGetter proxy_config_getter_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNetworkDelegate);
};

#endif  // CHROME_BROWSER_NET_CHROME_NETWORK_DELEGATE_H_
