// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_FIXED_HOST_RESOLVER_H_
#define NET_BASE_FIXED_HOST_RESOLVER_H_

#include <string>

#include "net/base/address_list.h"
#include "net/base/host_resolver.h"

namespace net {

// A FixedHostResolver resolves all addresses to a single address.
class FixedHostResolver : public HostResolver {
 public:
  // |host| is a string representing the resolution.
  //   example: foo.myproxy.com
  explicit FixedHostResolver(const std::string& host);

  // HostResolver methods:
  virtual int Resolve(const RequestInfo& info,
                      AddressList* addresses,
                      CompletionCallback* callback,
                      RequestHandle* out_req,
                      LoadLog* load_log);
  virtual void CancelRequest(RequestHandle req) {}
  virtual void AddObserver(Observer* observer) {}
  virtual void RemoveObserver(Observer* observer) {}
  virtual HostCache* GetHostCache() { return NULL; }
  virtual void Shutdown() {}

 private:
  ~FixedHostResolver() {}

  AddressList address_;
  bool initialized_;
};

}  // namespace net

#endif  // NET_BASE_MOCK_HOST_RESOLVER_H_
