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
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "net/base/network_delegate_impl.h"

class ChromeExtensionsNetworkDelegate;
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

namespace domain_reliability {
class DomainReliabilityMonitor;
}

namespace extensions {
class EventRouterForwarder;
class InfoMap;
}

namespace net {
class URLRequest;
}

namespace policy {
class URLBlacklistManager;
}

// ChromeNetworkDelegate is the central point from within the chrome code to
// add hooks into the network stack.
class ChromeNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  // |enable_referrers| (and all of the other optional PrefMembers) should be
  // initialized on the UI thread (see below) beforehand. This object's owner is
  // responsible for cleaning them up at shutdown.
  ChromeNetworkDelegate(extensions::EventRouterForwarder* event_router,
                        BooleanPrefMember* enable_referrers);
  ~ChromeNetworkDelegate() override;

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

  void set_force_safe_search(
      BooleanPrefMember* force_safe_search) {
    force_safe_search_ = force_safe_search;
  }

  void set_force_google_safe_search(
      BooleanPrefMember* force_google_safe_search) {
    force_google_safe_search_ = force_google_safe_search;
  }

  void set_force_youtube_safety_mode(
      BooleanPrefMember* force_youtube_safety_mode) {
    force_youtube_safety_mode_ = force_youtube_safety_mode;
  }

  void set_domain_reliability_monitor(
      domain_reliability::DomainReliabilityMonitor* monitor) {
    domain_reliability_monitor_ = monitor;
  }

  // Causes |OnCanThrottleRequest| to always return false, for all
  // instances of this object.
  static void NeverThrottleRequests();

  // Binds the pref members to |pref_service| and moves them to the IO thread.
  // |enable_referrers| cannot be NULL, the others can.
  // This method should be called on the UI thread.
  static void InitializePrefsOnUIThread(
      BooleanPrefMember* enable_referrers,
      BooleanPrefMember* enable_do_not_track,
      BooleanPrefMember* force_safe_search,
      BooleanPrefMember* force_google_safe_search,
      BooleanPrefMember* force_youtube_safety_mode,
      PrefService* pref_service);

  // When called, all file:// URLs will now be accessible.  If this is not
  // called, then some platforms restrict access to file:// paths.
  static void AllowAccessToAllFiles();

 private:
  friend class ChromeNetworkDelegateThrottlingTest;

  // NetworkDelegate implementation.
  int OnBeforeURLRequest(net::URLRequest* request,
                         const net::CompletionCallback& callback,
                         GURL* new_url) override;
  int OnBeforeSendHeaders(net::URLRequest* request,
                          const net::CompletionCallback& callback,
                          net::HttpRequestHeaders* headers) override;
  void OnSendHeaders(net::URLRequest* request,
                     const net::HttpRequestHeaders& headers) override;
  int OnHeadersReceived(
      net::URLRequest* request,
      const net::CompletionCallback& callback,
      const net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
      GURL* allowed_unsafe_redirect_url) override;
  void OnBeforeRedirect(net::URLRequest* request,
                        const GURL& new_location) override;
  void OnResponseStarted(net::URLRequest* request) override;
  void OnRawBytesRead(const net::URLRequest& request, int bytes_read) override;
  void OnCompleted(net::URLRequest* request, bool started) override;
  void OnURLRequestDestroyed(net::URLRequest* request) override;
  void OnPACScriptError(int line_number, const base::string16& error) override;
  net::NetworkDelegate::AuthRequiredResponse OnAuthRequired(
      net::URLRequest* request,
      const net::AuthChallengeInfo& auth_info,
      const AuthCallback& callback,
      net::AuthCredentials* credentials) override;
  bool OnCanGetCookies(const net::URLRequest& request,
                       const net::CookieList& cookie_list) override;
  bool OnCanSetCookie(const net::URLRequest& request,
                      const std::string& cookie_line,
                      net::CookieOptions* options) override;
  bool OnCanAccessFile(const net::URLRequest& request,
                       const base::FilePath& path) const override;
  bool OnCanThrottleRequest(const net::URLRequest& request) const override;
  bool OnCanEnablePrivacyMode(
      const GURL& url,
      const GURL& first_party_for_cookies) const override;
  bool OnFirstPartyOnlyCookieExperimentEnabled() const override;
  bool OnCancelURLRequestWithPolicyViolatingReferrerHeader(
      const net::URLRequest& request,
      const GURL& target_url,
      const GURL& referrer_url) const override;

  void AccumulateContentLength(
      int64 received_payload_byte_count,
      int64 original_payload_byte_count);

  scoped_ptr<ChromeExtensionsNetworkDelegate> extensions_delegate_;

  void* profile_;
  base::FilePath profile_path_;
  scoped_refptr<CookieSettings> cookie_settings_;

  scoped_ptr<chrome_browser_net::ConnectInterceptor> connect_interceptor_;

  // Weak, owned by our owner.
  BooleanPrefMember* enable_referrers_;
  BooleanPrefMember* enable_do_not_track_;
  BooleanPrefMember* force_safe_search_;
  BooleanPrefMember* force_google_safe_search_;
  BooleanPrefMember* force_youtube_safety_mode_;

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

  bool experimental_web_platform_features_enabled_;

  bool first_request_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNetworkDelegate);
};

#endif  // CHROME_BROWSER_NET_CHROME_NETWORK_DELEGATE_H_
