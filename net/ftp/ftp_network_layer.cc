// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_network_layer.h"

#include "net/ftp/ftp_network_session.h"
#include "net/ftp/ftp_network_transaction.h"
#include "net/socket/client_socket_factory.h"

namespace net {

FtpNetworkLayer::FtpNetworkLayer(HostResolver* host_resolver)
    : session_(new FtpNetworkSession(host_resolver)),
      suspended_(false) {
}

FtpNetworkLayer::~FtpNetworkLayer() {
}

scoped_ptr<FtpTransaction> FtpNetworkLayer::CreateTransaction() {
  if (suspended_)
    return scoped_ptr<FtpTransaction>();

  return make_scoped_ptr(new FtpNetworkTransaction(
      session_->host_resolver(), ClientSocketFactory::GetDefaultFactory()));
}

void FtpNetworkLayer::Suspend(bool suspend) {
  suspended_ = suspend;
}

}  // namespace net
