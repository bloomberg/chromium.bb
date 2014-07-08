// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/net/service_url_request_context_getter.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include <sys/utsname.h>
#endif

#include "base/compiler_specific.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/service/service_process.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context_builder.h"

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

// Returns the default user agent.
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

ServiceURLRequestContextGetter::ServiceURLRequestContextGetter()
    : user_agent_(MakeUserAgentForServiceProcess()),
      network_task_runner_(
          g_service_process->io_thread()->message_loop_proxy()) {
  // TODO(sanjeevr): Change CreateSystemProxyConfigService to accept a
  // MessageLoopProxy* instead of MessageLoop*.
  DCHECK(g_service_process);
  proxy_config_service_.reset(net::ProxyService::CreateSystemProxyConfigService(
      g_service_process->io_thread()->message_loop_proxy().get(),
      g_service_process->file_thread()->message_loop()));
}

net::URLRequestContext*
ServiceURLRequestContextGetter::GetURLRequestContext() {
  if (!url_request_context_.get()) {
    net::URLRequestContextBuilder builder;
    builder.set_user_agent(user_agent_);
    builder.set_accept_language("en-us,fr");
    builder.set_proxy_config_service(proxy_config_service_.release());
    builder.set_throttling_enabled(true);
    url_request_context_.reset(builder.Build());
  }
  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
ServiceURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

ServiceURLRequestContextGetter::~ServiceURLRequestContextGetter() {}
