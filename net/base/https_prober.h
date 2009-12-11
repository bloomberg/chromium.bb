// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_HTTPS_PROBER_H_
#define NET_BASE_HTTPS_PROBER_H_

#include <map>
#include <set>
#include <string>

#include "base/singleton.h"
#include "base/task.h"
#include "net/url_request/url_request.h"

class URLRequestContext;

namespace net {

// This should be scoped inside HTTPSProber, but VC cannot compile
// HTTPProber::Delegate when HTTPSProber also inherits from
// URLRequest::Delegate.
class HTTPSProberDelegate {
 public:
  virtual void ProbeComplete(bool result) = 0;
};

// HTTPSProber is a singleton object that manages HTTPS probes. A HTTPS probe
// determines if we can connect to a given host over HTTPS. It's used when
// transparently upgrading from HTTP to HTTPS (for example, for SPDY).
class HTTPSProber : public URLRequest::Delegate {
 public:
  HTTPSProber() { }

  // HaveProbed returns true if the given host is known to have been probed
  // since the browser was last started.
  bool HaveProbed(const std::string& host) const;

  // InFlight returns true iff a probe for the given host is currently active.
  bool InFlight(const std::string& host) const;

  // ProbeHost starts a new probe for the given host. If the host is known to
  // have been probed since the browser was started, false is returned and no
  // other action is taken. If a probe to the given host in currently inflight,
  // false will be returned, and no other action is taken. Otherwise, a new
  // probe is started, true is returned and the Delegate will be called with the
  // results (true means a successful handshake).
  bool ProbeHost(const std::string& host, URLRequestContext* ctx,
                 HTTPSProberDelegate* delegate);

  // Implementation of URLRequest::Delegate
  void OnAuthRequired(URLRequest* request,
                      net::AuthChallengeInfo* auth_info);
  void OnSSLCertificateError(URLRequest* request,
                             int cert_error,
                             net::X509Certificate* cert);
  void OnResponseStarted(URLRequest* request);
  void OnReadCompleted(URLRequest* request, int bytes_read);

 private:
  void Success(URLRequest* request);
  void Failure(URLRequest* request);
  void DoCallback(URLRequest* request, bool result);

  std::map<std::string, HTTPSProberDelegate*> inflight_probes_;
  std::set<std::string> probed_;

  friend struct DefaultSingletonTraits<HTTPSProber>;
  DISALLOW_EVIL_CONSTRUCTORS(HTTPSProber);
};

}  // namespace net
#endif
