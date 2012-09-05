// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/net/service_url_request_context.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include <sys/utsname.h>
#endif

#include "base/compiler_specific.h"
#include "base/message_loop_proxy.h"
#include "base/stringprintf.h"
#include "base/sys_info.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/service/service_process.h"
#include "net/base/cert_verifier.h"
#include "net/base/host_resolver.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/cookies/cookie_monster.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_throttler_manager.h"

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
    const std::string& user_agent,
    net::ProxyConfigService* net_proxy_config_service)
    : user_agent_(user_agent),
      ALLOW_THIS_IN_INITIALIZER_LIST(storage_(this)) {
  storage_.set_host_resolver(
      net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism,
                                    net::HostResolver::kDefaultRetryAttempts,
                                    NULL));
  storage_.set_proxy_service(net::ProxyService::CreateUsingSystemProxyResolver(
      net_proxy_config_service, 0u, NULL));
  storage_.set_cert_verifier(net::CertVerifier::CreateDefault());
  storage_.set_ftp_transaction_factory(
      new net::FtpNetworkLayer(host_resolver()));
  storage_.set_ssl_config_service(new net::SSLConfigServiceDefaults);
  storage_.set_http_auth_handler_factory(
      net::HttpAuthHandlerFactory::CreateDefault(host_resolver()));
  storage_.set_http_server_properties(new net::HttpServerPropertiesImpl);
  storage_.set_transport_security_state(new net::TransportSecurityState);
  storage_.set_throttler_manager(new net::URLRequestThrottlerManager);

  net::HttpNetworkSession::Params session_params;
  session_params.host_resolver = host_resolver();
  session_params.cert_verifier = cert_verifier();
  session_params.proxy_service = proxy_service();
  session_params.ssl_config_service = ssl_config_service();
  session_params.http_auth_handler_factory = http_auth_handler_factory();
  session_params.http_server_properties = http_server_properties();
  scoped_refptr<net::HttpNetworkSession> network_session(
      new net::HttpNetworkSession(session_params));
  storage_.set_http_transaction_factory(
      new net::HttpCache(
          network_session,
          net::HttpCache::DefaultBackend::InMemory(0)));
  // In-memory cookie store.
  storage_.set_cookie_store(new net::CookieMonster(NULL, NULL));
  set_accept_language("en-us,fr");
  set_accept_charset("iso-8859-1,*,utf-8");
}

const std::string& ServiceURLRequestContext::GetUserAgent(
    const GURL& url) const {
  // If the user agent is set explicitly return that, otherwise call the
  // base class method to return default value.
  return user_agent_.empty() ?
      net::URLRequestContext::GetUserAgent(url) : user_agent_;
}

ServiceURLRequestContext::~ServiceURLRequestContext() {
}

ServiceURLRequestContextGetter::ServiceURLRequestContextGetter()
    : network_task_runner_(
          g_service_process->io_thread()->message_loop_proxy()) {
  // Build the default user agent.
  user_agent_ = MakeUserAgentForServiceProcess();

  // TODO(sanjeevr): Change CreateSystemProxyConfigService to accept a
  // MessageLoopProxy* instead of MessageLoop*.
  DCHECK(g_service_process);
  proxy_config_service_.reset(
      net::ProxyService::CreateSystemProxyConfigService(
          g_service_process->io_thread()->message_loop_proxy(),
          g_service_process->file_thread()->message_loop()));
}

net::URLRequestContext*
ServiceURLRequestContextGetter::GetURLRequestContext() {
  if (!url_request_context_.get())
    url_request_context_.reset(
        new ServiceURLRequestContext(user_agent_,
                                     proxy_config_service_.release()));
  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
ServiceURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

ServiceURLRequestContextGetter::~ServiceURLRequestContextGetter() {}
