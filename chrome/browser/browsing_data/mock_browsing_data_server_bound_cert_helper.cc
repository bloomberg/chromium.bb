// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/mock_browsing_data_server_bound_cert_helper.h"

#include "base/logging.h"

MockBrowsingDataServerBoundCertHelper::MockBrowsingDataServerBoundCertHelper()
    : BrowsingDataServerBoundCertHelper() {}

MockBrowsingDataServerBoundCertHelper::
~MockBrowsingDataServerBoundCertHelper() {}

void MockBrowsingDataServerBoundCertHelper::StartFetching(
    const FetchResultCallback& callback) {
  callback_ = callback;
}

void MockBrowsingDataServerBoundCertHelper::DeleteServerBoundCert(
    const std::string& server_id) {
  CHECK(server_bound_certs_.find(server_id) != server_bound_certs_.end());
  server_bound_certs_[server_id] = false;
}

void MockBrowsingDataServerBoundCertHelper::AddServerBoundCertSample(
    const std::string& server_id) {
  DCHECK(server_bound_certs_.find(server_id) == server_bound_certs_.end());
  server_bound_cert_list_.push_back(
      net::ServerBoundCertStore::ServerBoundCert(
          server_id, base::Time(), base::Time(), "key", "cert"));
  server_bound_certs_[server_id] = true;
}

void MockBrowsingDataServerBoundCertHelper::Notify() {
  net::ServerBoundCertStore::ServerBoundCertList cert_list;
  for (net::ServerBoundCertStore::ServerBoundCertList::iterator i =
       server_bound_cert_list_.begin();
       i != server_bound_cert_list_.end(); ++i) {
    if (server_bound_certs_[i->server_identifier()])
      cert_list.push_back(*i);
  }
  callback_.Run(cert_list);
}

void MockBrowsingDataServerBoundCertHelper::Reset() {
  for (std::map<const std::string, bool>::iterator i =
       server_bound_certs_.begin();
       i != server_bound_certs_.end(); ++i)
    i->second = true;
}

bool MockBrowsingDataServerBoundCertHelper::AllDeleted() {
  for (std::map<const std::string, bool>::const_iterator i =
       server_bound_certs_.begin();
       i != server_bound_certs_.end(); ++i)
    if (i->second)
      return false;
  return true;
}
