// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_NETWORK_DELEGATE_H_
#define CHROMECAST_BROWSER_CAST_NETWORK_DELEGATE_H_

#include "base/macros.h"
#include "net/base/network_delegate_impl.h"

namespace chromecast {
namespace shell {

class CastNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  static std::unique_ptr<CastNetworkDelegate> Create();

  CastNetworkDelegate();
  ~CastNetworkDelegate() override;

  virtual void Initialize() = 0;

  virtual bool IsWhitelisted(const GURL& gurl,
                             const std::string& session_id,
                             int render_process_id,
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
