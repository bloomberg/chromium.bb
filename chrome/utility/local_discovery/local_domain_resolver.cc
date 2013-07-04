// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/utility/local_discovery/local_domain_resolver.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/record_parsed.h"
#include "net/dns/record_rdata.h"

namespace local_discovery {

LocalDomainResolver::LocalDomainResolver(net::MDnsClient* mdns_client,
                                         const std::string& domain,
                                         net::AddressFamily address_family,
                                         const IPAddressCallback& callback)
    : domain_(domain), address_family_(address_family), callback_(callback),
      transaction_failures_(0), mdns_client_(mdns_client) {
}

LocalDomainResolver::~LocalDomainResolver() {
}

bool LocalDomainResolver::Start() {
  if (address_family_ == net::ADDRESS_FAMILY_IPV4 ||
      address_family_ == net::ADDRESS_FAMILY_UNSPECIFIED) {
    transaction_a_ = CreateTransaction(net::dns_protocol::kTypeA);
    if (!transaction_a_->Start())
      return false;
  }

  if (address_family_ == net::ADDRESS_FAMILY_IPV6 ||
      address_family_ == net::ADDRESS_FAMILY_UNSPECIFIED) {
    transaction_aaaa_ = CreateTransaction(net::dns_protocol::kTypeAAAA);
    if (!transaction_aaaa_->Start())
      return false;
  }

  return true;
}

scoped_ptr<net::MDnsTransaction> LocalDomainResolver::CreateTransaction(
    uint16 type) {
  return mdns_client_->CreateTransaction(
      type, domain_, net::MDnsTransaction::SINGLE_RESULT |
                     net::MDnsTransaction::QUERY_CACHE |
                     net::MDnsTransaction::QUERY_NETWORK,
      base::Bind(&LocalDomainResolver::OnTransactionComplete,
                 base::Unretained(this)));
}

void LocalDomainResolver::OnTransactionComplete(
    net::MDnsTransaction::Result result, const net::RecordParsed* record) {
  if (result != net::MDnsTransaction::RESULT_RECORD &&
      address_family_ == net::ADDRESS_FAMILY_UNSPECIFIED) {
    transaction_failures_++;

    if (transaction_failures_ < 2) {
      return;
    }
  }

  transaction_a_.reset();
  transaction_aaaa_.reset();

  net::IPAddressNumber address;
  if (result == net::MDnsTransaction::RESULT_RECORD) {
    if (record->type() == net::dns_protocol::kTypeA) {
      const net::ARecordRdata* rdata = record->rdata<net::ARecordRdata>();
      DCHECK(rdata);
      address = rdata->address();
    } else {
      DCHECK(record->type() == net::dns_protocol::kTypeAAAA);
      const net::AAAARecordRdata* rdata = record->rdata<net::AAAARecordRdata>();
      address = rdata->address();
    }
  }

  callback_.Run(result == net::MDnsTransaction::RESULT_RECORD, address);
}

}  // namespace local_discovery
