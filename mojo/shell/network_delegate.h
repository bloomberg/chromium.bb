// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_NETWORK_DELEGATE_H_
#define MOJO_SHELL_NETWORK_DELEGATE_H_

#include "base/compiler_specific.h"
#include "net/base/network_delegate.h"

namespace mojo {
namespace shell {

class NetworkDelegate : public net::NetworkDelegate {
 public:
  NetworkDelegate();
  virtual bool OnCanAccessFile(const net::URLRequest& request,
                               const base::FilePath& path) const OVERRIDE;
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_MOJO_NETWORK_DELEGATE_H_
