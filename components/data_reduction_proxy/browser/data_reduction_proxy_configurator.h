// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_CONFIGURATOR_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_CONFIGURATOR_H_

#include <string>

#include "base/macros.h"

namespace data_reduction_proxy {

// Interface for enabling and disabling the data reduction proxy configuration,
// and for adding bypass rules. This is the interface that is used to set the
// networking configuration that causes traffic to be proxied.
class DataReductionProxyConfigurator {
 public:
  DataReductionProxyConfigurator() {}
  virtual ~DataReductionProxyConfigurator() {}

  // Enable the data reduction proxy. If |restricted|, only the fallback_origin
  // will be used.
  virtual void Enable(bool restricted,
                      const std::string& primary_origin,
                      const std::string& fallback_origin) = 0;

  // Disable the data reduction proxy.
  virtual void Disable() = 0;

  // Adds a host pattern to bypass. This should follow the same syntax used
  // in net::ProxyBypassRules; that is, a hostname pattern, a hostname suffix
  // pattern, an IP literal, a CIDR block, or the magic string '<local>'.
  // Bypass settings persist for the life of this object and are applied
  // each time the proxy is enabled, but are not updated while it is enabled.
  virtual void AddHostPatternToBypass(const std::string& pattern) = 0;

  // Adds a URL pattern to bypass the proxy. The base implementation strips
  // everything in |pattern| after the first single slash and then treats it
  // as a hostname pattern.
  virtual void AddURLPatternToBypass(const std::string& pattern) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyConfigurator);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_CONFIGURATOR_H_
