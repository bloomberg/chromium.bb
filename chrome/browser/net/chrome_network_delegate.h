// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_NETWORK_DELEGATE_H_
#define CHROME_BROWSER_NET_CHROME_NETWORK_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "net/base/network_delegate.h"

class CookieSettings;
class ExtensionInfoMap;
class ManagedModeURLFilter;
class PrefService;
template<class T> class PrefMember;

typedef PrefMember<bool> BooleanPrefMember;

namespace chrome_browser_net {
class LoadTimeStats;
}

namespace extensions {
class EventRouterForwarder;
}

namespace policy {
class URLBlacklistManager;
}

// ChromeNetworkDelegate is the central point from within the chrome code to
// add hooks into the network stack.
class ChromeNetworkDelegate : public net::NetworkDelegate {
 public:
  // If |profile| is NULL, events will be broadcasted to all profiles,
  // otherwise they will only be sent to the specified profile.
  // |enable_referrers| and |enable_do_not_track| should be initialized on the
  // UI thread (see below) beforehand. This object's owner is responsible for
  // cleaning it up at shutdown. |enable_do_not_track| can be NULL.
  // If |cookie_settings| is NULL, all cookies are enabled, otherwise, the
  // settings are enforced on all observed network requests.
  ChromeNetworkDelegate(
      extensions::EventRouterForwarder* event_router,
      ExtensionInfoMap* extension_info_map,
      const policy::URLBlacklistManager* url_blacklist_manager,
      const ManagedModeURLFilter* managed_mode_url_filter,
      void* profile,
      CookieSettings* cookie_settings,
      BooleanPrefMember* enable_referrers,
      BooleanPrefMember* enable_do_not_track,
      chrome_browser_net::LoadTimeStats* load_time_stats);
  virtual ~ChromeNetworkDelegate();

  // Causes |OnCanThrottleRequest| to always return false, for all
  // instances of this object.
  static void NeverThrottleRequests();

  // Binds the pref members to |pref_service| and moves them to the IO thread.
  // |enable_do_not_track| can be NULL.
  // This method should be called on the UI thread.
  static void InitializePrefsOnUIThread(BooleanPrefMember* enable_referrers,
                                        BooleanPrefMember* enable_do_not_track,
                                        PrefService* pref_service);

  // When called, all file:// URLs will now be accessible.  If this is not
  // called, then some platforms restrict access to file:// paths.
  static void AllowAccessToAllFiles();

 private:
  friend class ChromeNetworkDelegateTest;

  // NetworkDelegate implementation.
  virtual int OnBeforeURLRequest(net::URLRequest* request,
                                 const net::CompletionCallback& callback,
                                 GURL* new_url) OVERRIDE;
  virtual int OnBeforeSendHeaders(net::URLRequest* request,
                                  const net::CompletionCallback& callback,
                                  net::HttpRequestHeaders* headers) OVERRIDE;
  virtual void OnSendHeaders(net::URLRequest* request,
                             const net::HttpRequestHeaders& headers) OVERRIDE;
  virtual int OnHeadersReceived(
      net::URLRequest* request,
      const net::CompletionCallback& callback,
      net::HttpResponseHeaders* original_response_headers,
      scoped_refptr<net::HttpResponseHeaders>* override_response_headers)
      OVERRIDE;
  virtual void OnBeforeRedirect(net::URLRequest* request,
                                const GURL& new_location) OVERRIDE;
  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE;
  virtual void OnRawBytesRead(const net::URLRequest& request,
                              int bytes_read) OVERRIDE;
  virtual void OnCompleted(net::URLRequest* request, bool started) OVERRIDE;
  virtual void OnURLRequestDestroyed(net::URLRequest* request) OVERRIDE;
  virtual void OnPACScriptError(int line_number,
                                const string16& error) OVERRIDE;
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
                               const FilePath& path) const OVERRIDE;
  virtual bool OnCanThrottleRequest(
      const net::URLRequest& request) const OVERRIDE;
  virtual int OnBeforeSocketStreamConnect(
      net::SocketStream* stream,
      const net::CompletionCallback& callback) OVERRIDE;
  virtual void OnRequestWaitStateChange(const net::URLRequest& request,
                                        RequestWaitState state) OVERRIDE;

  scoped_refptr<extensions::EventRouterForwarder> event_router_;
  void* profile_;
  scoped_refptr<CookieSettings> cookie_settings_;

  scoped_refptr<ExtensionInfoMap> extension_info_map_;

  // Weak, owned by our owner.
  BooleanPrefMember* enable_referrers_;
  BooleanPrefMember* enable_do_not_track_;

  // Weak, owned by our owner.
  const policy::URLBlacklistManager* url_blacklist_manager_;

  // Weak pointer. The owner of this object needs to make sure that the
  // |managed_mode_url_filter_| outlives it.
  const ManagedModeURLFilter* managed_mode_url_filter_;

  // When true, allow access to all file:// URLs.
  static bool g_allow_file_access_;

  // True if OnCanThrottleRequest should always return false.
  //
  // Note: This needs to be static as the instance of
  // ChromeNetworkDelegate used may change over time, and we need to
  // set this variable once at start-up time.  It is effectively
  // static anyway since it is based on a command-line flag.
  static bool g_never_throttle_requests_;

  // Pointer to IOThread global, should outlive ChromeNetworkDelegate.
  chrome_browser_net::LoadTimeStats* load_time_stats_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNetworkDelegate);
};

#endif  // CHROME_BROWSER_NET_CHROME_NETWORK_DELEGATE_H_
