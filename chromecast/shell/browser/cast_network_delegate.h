// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_SHELL_BROWSER_CAST_NETWORK_DELEGATE_H_
#define CHROMECAST_SHELL_BROWSER_CAST_NETWORK_DELEGATE_H_

#include "net/base/network_delegate.h"

namespace net {
class X509Certificate;
}

namespace chromecast {
namespace shell {

class CastNetworkDelegate : public net::NetworkDelegate {
 public:
  static CastNetworkDelegate* Create();
  static net::X509Certificate* DeviceCert();

  CastNetworkDelegate();
  virtual ~CastNetworkDelegate();

  virtual void Initialize(bool use_sync_signing) = 0;

  virtual bool IsWhitelisted(const GURL& gurl, bool for_device_auth) const = 0;

 private:
  // net::NetworkDelegate implementation:
  virtual bool OnCanAccessFile(const net::URLRequest& request,
                               const base::FilePath& path) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(CastNetworkDelegate);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_SHELL_BROWSER_CAST_NETWORK_DELEGATE_H_
