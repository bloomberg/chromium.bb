// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SSL_SSL_HOST_STATE_H_
#define CONTENT_BROWSER_SSL_SSL_HOST_STATE_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/supports_user_data.h"
#include "base/threading/non_thread_safe.h"
#include "content/common/content_export.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"

namespace content {
class BrowserContext;
class SSLHostStateDelegate;

// SSLHostState
//
// The SSLHostState encapulates the host-specific state for SSL errors.  For
// example, SSLHostState remembers whether the user has whitelisted a
// particular broken cert for use with particular host.  We separate this state
// from the SSLManager because this state is shared across many navigation
// controllers.
class CONTENT_EXPORT SSLHostState
    : NON_EXPORTED_BASE(base::SupportsUserData::Data),
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  // Contexts may specify a NULL certificate decision storage strategy. In that
  // case, the returned SSLHostState from GetFor() will implement a default
  // strategy of ignoring all exception requests and returning
  // net::QueryPolicy::Judgment::UNKOWN from QueryPolicy().
  static SSLHostState* GetFor(BrowserContext* browser_context);

  SSLHostState();
  virtual ~SSLHostState();

  // Records that a host has run insecure content.
  void HostRanInsecureContent(const std::string& host, int pid);

  // Returns whether the specified host ran insecure content.
  bool DidHostRunInsecureContent(const std::string& host, int pid) const;

  // Records that |cert| is not permitted to be used for |url| in the future,
  // for a specified |error| type.
  void DenyCertForHost(net::X509Certificate* cert,
                       const std::string& host,
                       net::CertStatus error);

  // Records that |cert| is permitted to be used for |url| in the future, for
  // a specified |error| type.
  void AllowCertForHost(net::X509Certificate* cert,
                        const std::string& host,
                        net::CertStatus error);

  // Revoke all allow/deny preferences for |url|.
  void RevokeAllowAndDenyPreferences(const std::string& host);

  bool HasAllowedOrDeniedCert(const std::string& host);

  // Clear all allow/deny preferences.
  void Clear();

  // Queries whether |cert| is allowed or denied for |url| and |error|.
  net::CertPolicy::Judgment QueryPolicy(net::X509Certificate* cert,
                                        const std::string& host,
                                        net::CertStatus error);

 private:
  // A BrokenHostEntry is a pair of (host, process_id) that indicates the host
  // contains insecure content in that renderer process.
  typedef std::pair<std::string, int> BrokenHostEntry;

  // Hosts which have been contaminated with insecure content in the
  // specified process.  Note that insecure content can travel between
  // same-origin frames in one processs but cannot jump between processes.
  std::set<BrokenHostEntry> ran_insecure_content_hosts_;

  // The certificate decision store. It may be NULL, depending on the browsing
  // context. This is owned by the browsing context.
  SSLHostStateDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(SSLHostState);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SSL_SSL_HOST_STATE_H_
