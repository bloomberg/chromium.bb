// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/rappor/sampling.h"

#include "chrome/browser/browser_process.h"
#include "components/rappor/rappor_service.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace rappor {

void SampleDomainAndRegistryFromGURL(const std::string& metric,
                                     const GURL& gurl) {
  if (!g_browser_process->rappor_service())
    return;
  g_browser_process->rappor_service()->RecordSample(
      metric,
      rappor::ETLD_PLUS_ONE_RAPPOR_TYPE,
      net::registry_controlled_domains::GetDomainAndRegistry(
          gurl, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
}

void SampleDomainAndRegistryFromHost(const std::string& metric,
                                     const std::string& host) {
  if (!g_browser_process->rappor_service())
    return;
  g_browser_process->rappor_service()->RecordSample(
      metric,
      rappor::ETLD_PLUS_ONE_RAPPOR_TYPE,
      net::registry_controlled_domains::GetDomainAndRegistry(
          host, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
}

}  // namespace rappor
