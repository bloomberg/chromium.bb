// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_variations_service_client.h"

#include "base/bind.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"

namespace android_webview {
namespace {

// Gets the version number to use for variations seed simulation. Must be called
// on a thread where IO is allowed.
base::Version GetVersionForSimulation() {
  base::AssertBlockingAllowed();
  return base::Version(version_info::GetVersionNumber());
}

}  // namespace

AwVariationsServiceClient::AwVariationsServiceClient() {}

AwVariationsServiceClient::~AwVariationsServiceClient() {}

std::string AwVariationsServiceClient::GetApplicationLocale() {
  return std::string();
}

base::Callback<base::Version(void)>
AwVariationsServiceClient::GetVersionForSimulationCallback() {
  return base::Bind(&GetVersionForSimulation);
}

net::URLRequestContextGetter*
AwVariationsServiceClient::GetURLRequestContext() {
  return nullptr;
}

network_time::NetworkTimeTracker*
AwVariationsServiceClient::GetNetworkTimeTracker() {
  return nullptr;
}

version_info::Channel AwVariationsServiceClient::GetChannel() {
  // TODO(kmilka): Investigate the proper value to return here so experiments
  // are correctly filtered.
  return version_info::Channel::UNKNOWN;
}

bool AwVariationsServiceClient::OverridesRestrictParameter(
    std::string* parameter) {
  return false;
}

}  // namespace android_webview
