// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/sth_distributor_provider.h"

#include "base/lazy_instance.h"
#include "net/cert/sth_distributor.h"

namespace chrome_browser_net {

namespace {
base::LazyInstance<std::unique_ptr<net::ct::STHDistributor>>
    global_sth_distributor = LAZY_INSTANCE_INITIALIZER;
}  // namespace

void SetGlobalSTHDistributor(
    std::unique_ptr<net::ct::STHDistributor> distributor) {
  global_sth_distributor.Get().swap(distributor);
}

net::ct::STHDistributor* GetGlobalSTHDistributor() {
  CHECK(global_sth_distributor.Get());
  return global_sth_distributor.Get().get();
}

}  // namespace chrome_browser_net
