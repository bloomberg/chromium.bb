// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_network_transaction_factory.h"

#include "chrome/browser/devtools/devtools_network_controller.h"
#include "chrome/browser/devtools/devtools_network_transaction.h"
#include "net/base/net_errors.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_transaction.h"

DevToolsNetworkTransactionFactory::DevToolsNetworkTransactionFactory(
    DevToolsNetworkController* controller,
    net::HttpNetworkSession* session)
    : controller_(controller),
      network_layer_(new net::HttpNetworkLayer(session)) {
}

DevToolsNetworkTransactionFactory::~DevToolsNetworkTransactionFactory() {
}

int DevToolsNetworkTransactionFactory::CreateTransaction(
    net::RequestPriority priority,
    scoped_ptr<net::HttpTransaction>* trans) {
  scoped_ptr<net::HttpTransaction> network_transaction;
  int rv = network_layer_->CreateTransaction(priority, &network_transaction);
  if (rv != net::OK) {
    return rv;
  }
  trans->reset(
      new DevToolsNetworkTransaction(controller_, network_transaction.Pass()));
  return net::OK;
}

net::HttpCache* DevToolsNetworkTransactionFactory::GetCache() {
  return network_layer_->GetCache();
}

net::HttpNetworkSession* DevToolsNetworkTransactionFactory::GetSession() {
  return network_layer_->GetSession();
}
