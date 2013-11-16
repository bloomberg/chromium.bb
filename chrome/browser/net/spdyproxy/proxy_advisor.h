// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SPDYPROXY_PROXY_ADVISOR_H_
#define CHROME_BROWSER_NET_SPDYPROXY_PROXY_ADVISOR_H_

#include <set>

#include "base/prefs/pref_member.h"
#include "chrome/browser/net/url_info.h"
#include "net/url_request/url_request.h"

namespace net {
class URLRequestContextGetter;
}

class PrefService;

// Accessory class for net/preconnect to be used in conjunction with the
// data reduction proxy. An instance of this class will accept Advise()
// calls and will send HEAD requests to an endpoint URL on the proxy
// which notify the proxy of preconnection opportunities.
// The HEAD requests have a header of the form:
//   Proxy-Host-Advisory: <motivation> <url>
// Where <motivation> is a string describing what is motivating the
// preconnection ('learned_referral', 'omnibox', etc.).
//
// The ProxyAdvisor owns the HEAD requests. Since we don't care about any
// responses from the requests, they are deleted as soon as a response
// code is received.
//
// ProxyAdvisor monitors the state of the proxy preference; if it is
// disabled, all in-flight requests are canceled.
//
// ProxyAdvisor instances should be created on the UI thread.
class ProxyAdvisor : public net::URLRequest::Delegate {
 public:
  ProxyAdvisor(PrefService* pref_service,
               net::URLRequestContextGetter* context_getter);
  virtual ~ProxyAdvisor();

  // net::URLRequest::Delegate callbacks.
  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE;
  virtual void OnReadCompleted(net::URLRequest* request,
                               int bytes_read) OVERRIDE;

  // Tell the advisor that |url| is being preconnected or pre-resolved and why.
  // If |url| would be proxied (according to WouldProxyURL()), the ProxyAdvisor
  // will send a HEAD request to the proxy, giving it an opportunity to
  // preconnect or pre-resolve hostnames prior to the browser sending actual
  // requests.
  // If WouldProxyURL returns a false positive, then Advise() will send an
  // advisory HEAD request to the proxy, but |url| will be fetched by the
  // browser directly.
  // |motivation| and |is_preconnect| are used to determine a motivation string
  // that is passed as part of the request (if |is_preconnect| is false, the
  // advisory is interprered as being of lower priority).
  // Advise() may only be called on the IO thread.
  virtual void Advise(const GURL& url,
              chrome_browser_net::UrlInfo::ResolutionMotivation motivation,
              bool is_preconnect);

  // Returns true if, under the current proxy settings, |url| is likely to be
  // proxied. This is quick and dirty, rather that doing full proxy resolution
  // (which may involve PAC file execution and checking bad proxy lists).
  // TODO(marq): Make this method part of DataReductionProxySettings.
  virtual bool WouldProxyURL(const GURL& url);

 private:
  // Removes |request| from |inflight_requests_|.
  void RequestComplete(net::URLRequest* request);

  // Checks prefs::kSpdyProxyAuthEnabled and updates |proxy_enabled_|
  // accordingly. If the proxy has turned off, cancels all inflight requests.
  void UpdateProxyState();

  scoped_refptr<net::URLRequestContextGetter> context_getter_;

  BooleanPrefMember proxy_pref_member_;

  std::set<net::URLRequest*> inflight_requests_;
};

#endif  // CHROME_BROWSER_NET_SPDYPROXY_PROXY_ADVISOR_H_

