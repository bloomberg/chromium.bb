// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NET_IOS_CHROME_NETWORK_DELEGATE_H_
#define IOS_CHROME_BROWSER_NET_IOS_CHROME_NETWORK_DELEGATE_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "net/base/network_delegate_impl.h"

class PrefService;

template <typename T>
class PrefMember;

typedef PrefMember<bool> BooleanPrefMember;

namespace domain_reliability {
class DomainReliabilityMonitor;
}

namespace policy {
class URLBlacklistManager;
}

// IOSChromeNetworkDelegate is the central point from within the Chrome code to
// add hooks into the network stack.
class IOSChromeNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  IOSChromeNetworkDelegate();
  ~IOSChromeNetworkDelegate() override;

#if defined(ENABLE_CONFIGURATION_POLICY)
  void set_url_blacklist_manager(
      const policy::URLBlacklistManager* url_blacklist_manager) {
    url_blacklist_manager_ = url_blacklist_manager;
  }
#endif

  // If |cookie_settings| is null or not set, all cookies are enabled,
  // otherwise the settings are enforced on all observed network requests.
  // Not inlined because we assign a scoped_refptr, which requires us to include
  // the header file. Here we just forward-declare it.
  void set_cookie_settings(content_settings::CookieSettings* cookie_settings) {
    cookie_settings_ = cookie_settings;
  }

  void set_enable_do_not_track(BooleanPrefMember* enable_do_not_track) {
    enable_do_not_track_ = enable_do_not_track;
  }

  void set_domain_reliability_monitor(
      domain_reliability::DomainReliabilityMonitor* monitor) {
    domain_reliability_monitor_ = monitor;
  }

  // Binds the pref members to |pref_service| and moves them to the IO thread.
  // This method should be called on the UI thread.
  static void InitializePrefsOnUIThread(BooleanPrefMember* enable_do_not_track,
                                        PrefService* pref_service);

 private:
  // NetworkDelegate implementation.
  int OnBeforeURLRequest(net::URLRequest* request,
                         const net::CompletionCallback& callback,
                         GURL* new_url) override;
  void OnBeforeRedirect(net::URLRequest* request,
                        const GURL& new_location) override;
  void OnCompleted(net::URLRequest* request, bool started) override;
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
  bool OnCanEnablePrivacyMode(
      const GURL& url,
      const GURL& first_party_for_cookies) const override;
  bool OnCancelURLRequestWithPolicyViolatingReferrerHeader(
      const net::URLRequest& request,
      const GURL& target_url,
      const GURL& referrer_url) const override;

  void AccumulateContentLength(int64 received_payload_byte_count,
                               int64 original_payload_byte_count);

  scoped_refptr<content_settings::CookieSettings> cookie_settings_;

  // Weak, owned by our owner.
  BooleanPrefMember* enable_do_not_track_;

// Weak, owned by our owner.
#if defined(ENABLE_CONFIGURATION_POLICY)
  const policy::URLBlacklistManager* url_blacklist_manager_;
#endif
  domain_reliability::DomainReliabilityMonitor* domain_reliability_monitor_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeNetworkDelegate);
};

#endif  // IOS_CHROME_BROWSER_NET_IOS_CHROME_NETWORK_DELEGATE_H_
