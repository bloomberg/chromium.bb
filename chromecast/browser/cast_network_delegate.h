// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_NETWORK_DELEGATE_H_
#define CHROMECAST_BROWSER_CAST_NETWORK_DELEGATE_H_

#include "base/macros.h"
#include "net/base/network_delegate_impl.h"

namespace net {
class SSLPrivateKey;
class X509Certificate;
}

namespace chromecast {
namespace shell {

class CastNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  static std::unique_ptr<CastNetworkDelegate> Create();
  static scoped_refptr<net::X509Certificate> DeviceCert();
  static scoped_refptr<net::SSLPrivateKey> DeviceKey();

  CastNetworkDelegate();
  ~CastNetworkDelegate() override;

  virtual void Initialize() = 0;

  virtual bool IsWhitelisted(const GURL& gurl, int render_process_id,
                             bool for_device_auth) const = 0;

 private:
  // net::NetworkDelegate implementation:
  bool OnCanAccessFile(const net::URLRequest& request,
                       const base::FilePath& original_path,
                       const base::FilePath& absolute_path) const override;

  DISALLOW_COPY_AND_ASSIGN(CastNetworkDelegate);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_NETWORK_DELEGATE_H_
