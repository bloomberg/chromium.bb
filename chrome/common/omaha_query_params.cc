// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/omaha_query_params.h"

#include "base/stringprintf.h"
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
#else
  #error "unknown arch"
#endif

const char kChrome[] = "chrome";
const char kChromeCrx[] = "chromecrx";
const char kChromiumCrx[] = "chromiumcrx";

const char* GetProdIdString(chrome::OmahaQueryParams::ProdId prod) {
  switch (prod) {
    case chrome::OmahaQueryParams::CHROME:
      return kChrome;
      break;
    case chrome::OmahaQueryParams::CHROMECRX:
      return kChromeCrx;
      break;
    case chrome::OmahaQueryParams::CHROMIUMCRX:
      return kChromiumCrx;
      break;
  }
  return kUnknown;
}

const char kStable[] = "stable";
const char kBeta[] = "beta";
const char kDev[] = "dev";
const char kCanary[] = "canary";

const char* GetChannelString() {
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

}  // namespace

namespace chrome {

std::string OmahaQueryParams::Get(ProdId prod) {
  return StringPrintf("os=%s&arch=%s&prod=%s&prodchannel=%s&prodversion=%s",
                      kOs,
                      kArch,
                      GetProdIdString(prod),
                      GetChannelString(),
                      chrome::VersionInfo().Version().c_str());
}

}  // namespace chrome
