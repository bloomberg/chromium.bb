// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/net/service_url_request_context.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include <sys/utsname.h>
#endif

#include "base/message_loop_proxy.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/service/service_process.h"
#include "net/base/cert_verifier.h"
#include "net/base/cookie_monster.h"
#include "net/base/cookie_policy.h"
#include "net/base/dnsrr_resolver.h"
#include "net/base/host_resolver.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/proxy/proxy_service.h"

namespace {
// Copied from webkit/glue/user_agent.cc. We don't want to pull in a dependency
// on webkit/glue which also pulls in the renderer. Also our user-agent is
// totally different from the user-agent used by the browser, just the
// OS-specific parts are common.
std::string BuildOSCpuInfo() {
  std::string os_cpu;

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
  int32 os_major_version = 0;
  int32 os_minor_version = 0;
  int32 os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&os_major_version,
                                               &os_minor_version,
                                               &os_bugfix_version);
#endif
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  // Should work on any Posix system.
  struct utsname unixinfo;
  uname(&unixinfo);

  std::string cputype;
  // special case for biarch systems
  if (strcmp(unixinfo.machine, "x86_64") == 0 &&
      sizeof(void*) == sizeof(int32)) {  // NOLINT
    cputype.assign("i686 (x86_64)");
  } else {
    cputype.assign(unixinfo.machine);
  }
#endif

  base::StringAppendF(
      &os_cpu,
#if defined(OS_WIN)
      "Windows NT %d.%d",
      os_major_version,
      os_minor_version
#elif defined(OS_MACOSX)
      "Intel Mac OS X %d_%d_%d",
      os_major_version,
      os_minor_version,
      os_bugfix_version
#elif defined(OS_CHROMEOS)
      "CrOS %s %d.%d.%d",
      cputype.c_str(),  // e.g. i686
      os_major_version,
      os_minor_version,
      os_bugfix_version
#else
      "%s %s",
      unixinfo.sysname,  // e.g. Linux
      cputype.c_str()    // e.g. i686
#endif
  );  // NOLINT

  return os_cpu;
}

std::string MakeUserAgentForServiceProcess() {
  std::string user_agent;
  chrome::VersionInfo version_info;
  if (!version_info.is_valid()) {
    DLOG(ERROR) << "Unable to create chrome::VersionInfo object";
  }
  std::string extra_version_info;
  if (!version_info.IsOfficialBuild())
    extra_version_info = "-devel";
  base::StringAppendF(&user_agent,
                      "Chrome Service %s(%s)%s %s ",
                      version_info.Version().c_str(),
                      version_info.LastChange().c_str(),
                      extra_version_info.c_str(),
                      BuildOSCpuInfo().c_str());
  return user_agent;
}

}  // namespace

ServiceURLRequestContext::ServiceURLRequestContext(
    const std::string& user_agent) : user_agent_(user_agent) {
  host_resolver_ =
      net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism,
                                    NULL, NULL);
  DCHECK(g_service_process);
  // TODO(sanjeevr): Change CreateSystemProxyConfigService to accept a
  // MessageLoopProxy* instead of MessageLoop*.
  // Also this needs to be created on the UI thread on Linux. Fix this.
  net::ProxyConfigService * proxy_config_service =
      net::ProxyService::CreateSystemProxyConfigService(
          g_service_process->io_thread()->message_loop(),
          g_service_process->file_thread()->message_loop());
  proxy_service_ = net::ProxyService::CreateUsingSystemProxyResolver(
      proxy_config_service, 0u, NULL);
  cert_verifier_ = new net::CertVerifier;
  dnsrr_resolver_ = new net::DnsRRResolver;
  ftp_transaction_factory_ = new net::FtpNetworkLayer(host_resolver_);
  ssl_config_service_ = new net::SSLConfigServiceDefaults;
  http_auth_handler_factory_ = net::HttpAuthHandlerFactory::CreateDefault(
      host_resolver_);
  net::HttpNetworkSession::Params session_params;
  session_params.host_resolver = host_resolver_;
  session_params.cert_verifier = cert_verifier_;
  session_params.dnsrr_resolver = dnsrr_resolver_;
  session_params.proxy_service = proxy_service_;
  session_params.ssl_config_service = ssl_config_service_;
  scoped_refptr<net::HttpNetworkSession> network_session(
      new net::HttpNetworkSession(session_params));
  http_transaction_factory_ = new net::HttpCache(
      network_session,
      net::HttpCache::DefaultBackend::InMemory(0));
  // In-memory cookie store.
  cookie_store_ = new net::CookieMonster(NULL, NULL);
  accept_language_ = "en-us,fr";
  accept_charset_ = "iso-8859-1,*,utf-8";
}

const std::string& ServiceURLRequestContext::GetUserAgent(
    const GURL& url) const {
  // If the user agent is set explicitly return that, otherwise call the
  // base class method to return default value.
  return user_agent_.empty() ?
      net::URLRequestContext::GetUserAgent(url) : user_agent_;
}

ServiceURLRequestContext::~ServiceURLRequestContext() {
  delete ftp_transaction_factory_;
  delete http_transaction_factory_;
  delete http_auth_handler_factory_;
  delete cert_verifier_;
  delete dnsrr_resolver_;
}

ServiceURLRequestContextGetter::ServiceURLRequestContextGetter()
    : io_message_loop_proxy_(
          g_service_process->io_thread()->message_loop_proxy()) {
  // Build the default user agent.
  user_agent_ = MakeUserAgentForServiceProcess();
}

net::URLRequestContext*
ServiceURLRequestContextGetter::GetURLRequestContext() {
  if (!url_request_context_)
    url_request_context_ = new ServiceURLRequestContext(user_agent_);
  return url_request_context_;
}

scoped_refptr<base::MessageLoopProxy>
ServiceURLRequestContextGetter::GetIOMessageLoopProxy() const {
  return io_message_loop_proxy_;
}

ServiceURLRequestContextGetter::~ServiceURLRequestContextGetter() {}
