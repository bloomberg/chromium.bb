// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_host_state.h"

#include "base/logging.h"
#include "base/lazy_instance.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"

namespace {
typedef std::map<content::BrowserContext*, SSLHostState*> HostStateMap;
static base::LazyInstance<HostStateMap> g_host_state_map =
    LAZY_INSTANCE_INITIALIZER;
}

SSLHostState* SSLHostState::GetFor(content::BrowserContext* browser_context) {
  if (!g_host_state_map.Get().count(browser_context))
    g_host_state_map.Get()[browser_context] = new SSLHostState(browser_context);
  return g_host_state_map.Get()[browser_context];
}

SSLHostState::SSLHostState(content::BrowserContext* browser_context) {
  registrar_.Add(this, content::NOTIFICATION_BROWSER_CONTEXT_DESTRUCTION,
                 content::Source<content::BrowserContext>(browser_context));
}

SSLHostState::~SSLHostState() {
}

void SSLHostState::HostRanInsecureContent(const std::string& host, int pid) {
  DCHECK(CalledOnValidThread());
  ran_insecure_content_hosts_.insert(BrokenHostEntry(host, pid));
}

bool SSLHostState::DidHostRunInsecureContent(const std::string& host,
                                             int pid) const {
  DCHECK(CalledOnValidThread());
  return !!ran_insecure_content_hosts_.count(BrokenHostEntry(host, pid));
}

void SSLHostState::DenyCertForHost(net::X509Certificate* cert,
                                   const std::string& host) {
  DCHECK(CalledOnValidThread());

  cert_policy_for_host_[host].Deny(cert);
}

void SSLHostState::AllowCertForHost(net::X509Certificate* cert,
                                    const std::string& host) {
  DCHECK(CalledOnValidThread());

  cert_policy_for_host_[host].Allow(cert);
}

net::CertPolicy::Judgment SSLHostState::QueryPolicy(
    net::X509Certificate* cert, const std::string& host) {
  DCHECK(CalledOnValidThread());

  return cert_policy_for_host_[host].Check(cert);
}

void SSLHostState::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  delete this;
}
