// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/callback.h"
#include "net/base/address_family.h"
#include "net/base/net_util.h"
#include "net/dns/mdns_client.h"

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_LOCAL_DOMAIN_RESOLVER_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_LOCAL_DOMAIN_RESOLVER_H_

namespace local_discovery {

class LocalDomainResolver {
 public:
  typedef base::Callback<void(bool, const net::IPAddressNumber&)>
      IPAddressCallback;

  // |mdns_client| must outlive the LocalDomainResolver.
  LocalDomainResolver(net::MDnsClient* mdns_client,
                      const std::string& domain,
                      net::AddressFamily address_family,
                      const IPAddressCallback& callback);
  ~LocalDomainResolver();

  bool Start();

  const std::string& domain() { return domain_; }

 private:
  void OnTransactionComplete(
      net::MDnsTransaction::Result result,
      const net::RecordParsed* record);

  scoped_ptr<net::MDnsTransaction> CreateTransaction(uint16 type);

  std::string domain_;
  net::AddressFamily address_family_;
  IPAddressCallback callback_;

  scoped_ptr<net::MDnsTransaction> transaction_a_;
  scoped_ptr<net::MDnsTransaction> transaction_aaaa_;

  int transaction_failures_;

  net::MDnsClient* mdns_client_;

  DISALLOW_COPY_AND_ASSIGN(LocalDomainResolver);
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_LOCAL_DOMAIN_RESOLVER_H_
