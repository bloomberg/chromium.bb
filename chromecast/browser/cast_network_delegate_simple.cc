// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_network_delegate.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_private_key.h"
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
  bool IsWhitelisted(const GURL& gurl, int render_process_id,
                     bool for_device_auth) const override {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(CastNetworkDelegateSimple);
};

}  // namespace

// static
std::unique_ptr<CastNetworkDelegate> CastNetworkDelegate::Create() {
  return base::MakeUnique<CastNetworkDelegateSimple>();
}

// static
scoped_refptr<net::X509Certificate> CastNetworkDelegate::DeviceCert() {
  return nullptr;
}

// static
scoped_refptr<net::SSLPrivateKey> CastNetworkDelegate::DeviceKey() {
  return nullptr;
}

}  // namespace shell
}  // namespace chromecast
