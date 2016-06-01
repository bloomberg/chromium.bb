// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_STH_DISTRIBUTOR_PROVIDER_H_
#define CHROME_BROWSER_NET_STH_DISTRIBUTOR_PROVIDER_H_

#include <memory>

namespace net {

namespace ct {
class STHDistributor;
}  // namespace ct

}  // namespace net

namespace chrome_browser_net {

void SetGlobalSTHDistributor(
    std::unique_ptr<net::ct::STHDistributor> distributor);
net::ct::STHDistributor* GetGlobalSTHDistributor();

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_STH_DISTRIBUTOR_PROVIDER_H_
