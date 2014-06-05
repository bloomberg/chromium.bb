// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_network_conditions.h"

#include "url/gurl.h"

DevToolsNetworkConditions::DevToolsNetworkConditions(
    const std::vector<std::string>& domains)
    : domains_(domains){
}

DevToolsNetworkConditions::~DevToolsNetworkConditions() {
}

bool DevToolsNetworkConditions::HasMatchingDomain(const GURL& url) const {
  Domains::const_iterator domain = domains_.begin();
  if (domain == domains_.end())
    return true;
  for (; domain != domains_.end(); ++domain) {
    if (url.DomainIs(domain->data()))
      return true;
  }
  return false;
}

bool DevToolsNetworkConditions::IsOffline() const {
  return true;
}
