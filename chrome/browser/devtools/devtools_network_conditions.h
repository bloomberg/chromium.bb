// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_CONDITIONS_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_CONDITIONS_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"

class GURL;

// DevToolsNetworkConditions holds information about desired network conditions.
class DevToolsNetworkConditions
    : public base::RefCounted<DevToolsNetworkConditions> {
 public:
  explicit DevToolsNetworkConditions(const std::vector<std::string>& domains);

  bool HasMatchingDomain(const GURL& url) const;
  bool IsOffline() const;

 private:
  friend class base::RefCounted<DevToolsNetworkConditions>;

  virtual ~DevToolsNetworkConditions();

  // List of domains that will be affected by network conditions.
  typedef std::vector<std::string> Domains;
  const Domains domains_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsNetworkConditions);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NETWORK_CONDITIONS_H_
