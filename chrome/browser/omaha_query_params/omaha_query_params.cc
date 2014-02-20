// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/omaha_query_params/omaha_query_params.h"

#include "base/compiler_specific.h"
#include "base/strings/stringprintf.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_version_info.h"

namespace {

const char kUnknown[] = "unknown";

// The request extra information is the OS and architecture, this helps
// the server select the right package to be delivered.
const char kOs[] =
#if defined(OS_MACOSX)
    "mac";
#elif defined(OS_WIN)
    "win";
#elif defined(OS_ANDROID)
    "android";
#elif defined(OS_CHROMEOS)
    "cros";
#elif defined(OS_LINUX)
    "linux";
#elif defined(OS_OPENBSD)
    "openbsd";
#else
#error "unknown os"
#endif

const char kArch[] =
#if defined(__amd64__) || defined(_WIN64)
    "x64";
#elif defined(__i386__) || defined(_WIN32)
    "x86";
#elif defined(__arm__)
    "arm";
#elif defined(__arm64__)
    "arm64";
#elif defined(__mips__)
    "mipsel";
#else
#error "unknown arch"
#endif

const char kChrome[] = "chrome";

const char kStable[] = "stable";
const char kBeta[] = "beta";
const char kDev[] = "dev";
const char kCanary[] = "canary";

#if defined(GOOGLE_CHROME_BUILD)
const char kChromeCrx[] = "chromecrx";
#else
const char kChromiumCrx[] = "chromiumcrx";
#endif  // defined(GOOGLE_CHROME_BUILD)

}  // namespace

namespace chrome {

// static
std::string OmahaQueryParams::Get(ProdId prod) {
  return base::StringPrintf(
      "os=%s&arch=%s&nacl_arch=%s&prod=%s&prodchannel=%s"
          "&prodversion=%s&lang=%s",
      kOs,
      kArch,
      GetNaclArch(),
      GetProdIdString(prod),
      GetChannelString(),
      chrome::VersionInfo().Version().c_str(),
      GetLang());
}

// static
const char* OmahaQueryParams::GetProdIdString(
    chrome::OmahaQueryParams::ProdId prod) {
  switch (prod) {
    case chrome::OmahaQueryParams::CHROME:
      return kChrome;
      break;
    case chrome::OmahaQueryParams::CRX:
#if defined(GOOGLE_CHROME_BUILD)
      return kChromeCrx;
#else
      return kChromiumCrx;
#endif
      break;
  }
  return kUnknown;
}

// static
const char* OmahaQueryParams::GetOS() {
  return kOs;
}

// static
const char* OmahaQueryParams::GetArch() {
  return kArch;
}

// static
const char* OmahaQueryParams::GetNaclArch() {
#if defined(ARCH_CPU_X86_FAMILY)
#if defined(ARCH_CPU_X86_64)
  return "x86-64";
#elif defined(OS_WIN)
  bool x86_64 = (base::win::OSInfo::GetInstance()->wow64_status() ==
                 base::win::OSInfo::WOW64_ENABLED);
  return x86_64 ? "x86-64" : "x86-32";
#else
  return "x86-32";
#endif
#elif defined(ARCH_CPU_ARMEL)
  return "arm";
#elif defined(ARCH_CPU_ARM64)
  return "arm64";
#elif defined(ARCH_CPU_MIPSEL)
  return "mips32";
#else
// NOTE: when adding new values here, please remember to update the
// comment in the .h file about possible return values from this function.
#error "You need to add support for your architecture here"
#endif
}

const char* OmahaQueryParams::GetChannelString() {
  switch (chrome::VersionInfo::GetChannel()) {
    case chrome::VersionInfo::CHANNEL_STABLE:
      return kStable;
      break;
    case chrome::VersionInfo::CHANNEL_BETA:
      return kBeta;
      break;
    case chrome::VersionInfo::CHANNEL_DEV:
      return kDev;
      break;
    case chrome::VersionInfo::CHANNEL_CANARY:
      return kCanary;
      break;
    case chrome::VersionInfo::CHANNEL_UNKNOWN:
      return kUnknown;
      break;
  }
  return kUnknown;
}

const char* OmahaQueryParams::GetLang() {
  return g_browser_process->GetApplicationLocale().c_str();
}

}  // namespace chrome
