// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chromecast/browser/cast_network_delegate.h"

#include "base/macros.h"
#include "url/gurl.h"

namespace chromecast {
namespace shell {

namespace {

class CastNetworkDelegateSimple : public CastNetworkDelegate {
 public:
  CastNetworkDelegateSimple() {}

 private:
  // CastNetworkDelegate implementation:
  void Initialize() override {}
  bool IsWhitelisted(const GURL& gurl,
                     const std::string& session_id,
                     int render_process_id,
                     bool for_device_auth) const override {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(CastNetworkDelegateSimple);
};

}  // namespace

// static
std::unique_ptr<CastNetworkDelegate> CastNetworkDelegate::Create() {
  return std::make_unique<CastNetworkDelegateSimple>();
}

}  // namespace shell
}  // namespace chromecast
